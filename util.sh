# Set what platform type to build.
# Set PLATFORM environment variable to override default behavior.
# Supported platform types - 'linux64', 'windows', 'osx', 'android'
# 'msys' is the git bash shell, built using mingw-w64, running under Microsoft Windows.
function set-platform() {
  # set PLATFORM to android on linux host to build android
  case "$OSTYPE" in
  darwin*)  PLATFORM=${PLATFORM:-osx} ;;
  linux*)   PLATFORM=${PLATFORM:-linux64} ;;
  win32*)   PLATFORM=${PLATFORM:-windows} ;;
  msys*)   PLATFORM=${PLATFORM:-windows} ;;
  *)        echo "Building on unsupported OS: $OSTYPE"; exit 1; ;;
  esac
}

# This cleans the output directory.
# $1: The output directory.
function clean() {
  local outdir="$1"
  rm -rf $outdir/* $outdir/.gclient*
}

# Makes sure depot tools are present.
# $1: The platform type.
# $2: The depot tools url.
# $3: The depot tools directory.
function check::depot-tools() {
  local platform="$1"
  local depot_tools_url="$2"
  local depot_tools_dir="$3"

  if [ ! -d $depot_tools_dir ]; then
    git clone -q $depot_tools_url $depot_tools_dir
    if [ $platform = 'windows' ]; then
      # run gclient.bat to get python
      pushd $depot_tools_dir
      ./gclient.bat
      popd
    fi
  fi
}

# Checks if a given package is installed.
# $1: The package name.
function check::is-package-installed() {
  local package="$1"
  dpkg-query --show --showformat='\r' $package
}

# Installs a given package.
# $1: The package name.
function check::install-package() {
  local package="$1"
  sudo apt-get install -y $package
}

# Makes sure all build dependencies are present.
# $1: The platform type.
# $2: The depot tools directory.
function check::deps() {
  local platform="$1"
  local depot_tools_dir="$2"

  case $platform in
  osx)
    # for GNU version of cp: gcp and jq
    which jq || brew install jq
    which gsed || brew install gnu-sed
    which gcp || brew install coreutils
    ;;
  linux*|android)
    packages="curl wget git jq python python-pip default-jdk g++ ruby
      libnss3-dev libasound2-dev libpulse-dev libjpeg-dev libxv-dev
      libgtk2.0-dev libexpat1-dev libxtst-dev libxss-dev libudev-dev
      libgconf2-dev libgnome-keyring-dev libpci-dev libgl1-mesa-dev lib32stdc++6
      lib32z1"
    for p in $packages; do
      check::is-package-installed $p || check::install-package $p
    done
    ;;
  windows)
    # force creation of the output directory, because curl --create-dirs does not seem to work
    mkdir --parents $depot_tools_dir/python276_bin
    # put jq in python_bin in depot_tools because it is ignored by git
    curl --location --create-dirs --output $depot_tools_dir/python276_bin/jq.exe https://github.com/stedolan/jq/releases/download/jq-1.4/jq-win32.exe
    ;;
  esac
}

# Checks out a specific revision
# $1: The platform type.
# $2: The output directory.
# $3: Revision represented as a git SHA.
function checkout() {
  local platform="$1"
  local outdir="$2"
  local revision="$3"

  pushd $outdir
  if [ ! -d src ]; then
    case $platform in
    android)
      fetch --nohooks webrtc_android
      ;;
    *)
      fetch --nohooks webrtc
      ;;
    esac
  fi
  # check out the specific revision after fetch
  gclient sync --force --revision $revision
  popd
}

# Patches a checkout for building static standalone libs
# $1: The platform type.
# $2: The output directory.
function patch() {
  local platform="$1"
  local outdir="$2"

  pushd $outdir
  case $platform in
  windows)
    # patch for directx not found
    sed -i 's|\(#include <d3dx9.h>\)|//\1|' $outdir/src/webrtc/modules/video_render/windows/video_render_direct3d9.h
    sed -i 's|\(D3DXMATRIX\)|//\1|' $outdir/src/webrtc/modules/video_render/windows/video_render_direct3d9.cc
    # patch all platforms to build standalone libs
    find src/webrtc src/talk src/chromium/src/third_party \( -name *.gyp -o  -name *.gypi \) -not -path *libyuv* -exec sed -i "s|\('type': 'static_library',\)|\1 'standalone_static_library': 1,|" '{}' ';'
    # also for the libs that say 'type': '<(component)' like nss and icu
    find src/chromium/src/third_party src/net/third_party/nss \( -name *.gyp -o  -name *.gypi \) -not -path *libyuv* -exec sed -i "s|\('type': '<(component)',\)|\1 'standalone_static_library': 1,|" '{}' ';'
    ;;
  android) ;;
  *)
    # sed
    if [ $platform = 'osx' ]; then
      SED='gsed'
    else
      SED='sed'
    fi
    # patch all platforms to build standalone libs
    find src/webrtc src/talk src/chromium/src/third_party \( -name *.gyp -o  -name *.gypi \) -not -path *libyuv* -exec $SED -i "s|\('type': 'static_library',\)|\1 'standalone_static_library': 1,|" '{}' ';'
    # for icu only; icu_use_data_file_flag is 1 on linux
    find src/chromium/src/third_party/icu/icu.gyp \( -name *.gyp -o  -name *.gypi \) -exec $SED -i "s|\('type': 'none',\)|\1 'standalone_static_library': 0,|" '{}' ';'
    # enable rtti for osx and linux
    $SED -i "s|'GCC_ENABLE_CPP_RTTI': 'NO'|'GCC_ENABLE_CPP_RTTI': 'YES'|" src/chromium/src/build/common.gypi
    $SED -i "s|^          '-fno-rtti'|          '-frtti'|" src/chromium/src/build/common.gypi
    # don't make thin archives
    $SED -i "s|, 'alink_thin'|, 'alink'|" src/tools/gyp/pylib/gyp/generator/ninja.py
    ;;
  esac
  popd
}

# This compiles the library.
# $1: The platform type.
# $2: The output directory.
function compile() {
  local platform="$1"
  local outdir="$2"

  pushd $outdir
  # start with no tests
  #GYP_DEFINES='build_with_chromium=0 include_tests=0 host_clang=0 disable_glibcxx_debug=1 linux_use_debug_fission=0'
  GYP_DEFINES='build_with_chromium=0 include_tests=0'

  case $platform in
  windows)
    # do the build
    # This is a hack to force use of Microsoft Visual C++ 2013.
    # More sophisticated code would look first for Visual C++ 2015, then 2013.
    GYP_MSVS_VERSION = '2013'
    cmd //c "$WIN_DEPOT_TOOLS\gclient.bat runhooks"
    ninja -C src/out/Debug
    ninja -C src/out/Release

    # 64-bit build
    GYP_DEFINES="target_arch=x64 $GYP_DEFINES"

    # do the build
    cmd //c "$WIN_DEPOT_TOOLS\gclient.bat runhooks"
    ninja -C src/out/Debug_x64
    ninja -C src/out/Release_x64

    # combine all the static libraries into one called webrtc_full
    # LIB.EXE /OUT:c.lib a.lib b.lib
    "$VS120COMNTOOLS../../VC/bin/lib" /OUT:src/out/Debug/webrtc_full.lib src/out/Debug/*.lib
    "$VS120COMNTOOLS../../VC/bin/lib" /OUT:src/out/Release/webrtc_full.lib src/out/Release/*.lib
    "$VS120COMNTOOLS../../VC/bin/lib" /OUT:src/out/Debug_x64/webrtc_full.lib src/out/Debug_x64/*.lib
    "$VS120COMNTOOLS../../VC/bin/lib" /OUT:src/out/Release_x64/webrtc_full.lib src/out/Release_x64/*.lib
    ;;
  *)
    if [ $platform = 'android' ]; then
      GYP_DEFINES="OS=android $GYP_DEFINES"
      . src/build/android/envsetup.sh
    elif [ $platform = 'osx' ]; then
      GYP_DEFINES="target_arch=x64 $GYP_DEFINES"
    fi
    # do the build
    configs="Debug Release"
    for cfg in $configs; do
      gclient runhooks
      ninja -C src/out/$cfg
      # combine all the static libraries into one called webrtc_full
      pushd src/out/$cfg
      find . -name '*.a' -exec ar -x '{}' ';'
      ar -crs libwebrtc_full.a *.o
      rm *.o
      popd
    done
    ;;
  esac
  popd
}

# This packages a compiled build into a zip file in the output directory.
# $1: The platform type.
# $2: The output directory.
# $3: Revision represented as a git SHA.
# $4: The project's resource dirctory.
function package() {
  local platform="$1"
  local outdir="$2"
  local revision="$3"
  local resourcedir="$4"

  # go into the webrtc repo to be able to get the revision number from git log
  pushd $outdir
  if [ $platform = 'Darwin' ]; then
    SED='gsed'
    CP='gcp'
  else
    SED='sed'
    CP='cp'
  fi
  pushd src
  local revision_number=$(git log -1 | tail -1 | $SED -ne 's|.*@\W*\([0-9]\+\).*$|\1|p')
  [[ -n $revision_number ]] || \
    { echo "Could not get revision number for packaging" && exit 1; }

  local revision_short=$(git rev-parse --short $revision)
  local label=$PROJECT_NAME-$revision_number-$revision_short-$platform
  # create directory structure
  mkdir -p $outdir/$label/include $outdir/$label/lib
  # find and copy header files
  find webrtc talk chromium/src/third_party/jsoncpp -name *.h \
    -exec $CP --parents '{}' $outdir/$label/include ';'
  # find and copy libraries
  pushd out
  find . -maxdepth 3 \( -name *.so -o -name *webrtc_full* -o -name *.jar \) \
    -exec $CP --parents '{}' $outdir/$label/lib ';'
  popd
  popd

  # for linux64, add pkgconfig files
  if [ $platform = 'linux64' ]; then
    configs="Debug Release"
    for cfg in $configs; do
      mkdir -p $label/lib/$cfg/pkgconfig
      CONFIG=$cfg envsubst '$CONFIG' < $resourcedir/pkgconfig/libwebrtc_full.pc.in > \
        $label/lib/$cfg/pkgconfig/libwebrtc_full.pc
    done
  fi
  # zip up the package
  if [ $platform = 'windows' ]; then
    $DEPOT_TOOLS/win_toolchain/7z/7z.exe a -tzip $label.zip $label
  else
    zip -r $label.zip $label
  fi
  # archive revision_number
  echo $revision_number > revision_number
  popd
}
