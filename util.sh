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
  msys*)    PLATFORM=${PLATFORM:-windows} ;;
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
      pushd $depot_tools_dir >/dev/null
      ./gclient.bat
      popd >/dev/null
    fi
  else
    pushd $depot_tools_dir >/dev/null
      git reset --hard
    popd >/dev/null
  fi
}

# Checks if a given package is installed.
# $1: The package name.
function check::is-package-installed() {
  local package="$1"
  dpkg -s $package &>/dev/null
}

# Installs a given package.
# $1: The package name.
function check::install-package() {
  local package="$1"
  sudo apt-get install -qq $package
}

# Makes sure all build dependencies are present.
# $1: The platform type.
function check::deps() {
  local platform="$1"

  case $platform in
  osx)
    # for GNU version of cp: gcp
    which gcp || brew install coreutils
    ;;
  linux*|android)
    if ! which sudo > /dev/null ; then
      apt-get update -qq && apt-get install -qq sudo
    fi
    packages="curl wget git python python-pip default-jdk g++ ruby
      libnss3-dev libasound2-dev libpulse-dev libjpeg-dev libxv-dev
      libgtk2.0-dev libexpat1-dev libxtst-dev libxss-dev libudev-dev
      libgconf2-dev libgnome-keyring-dev libpci-dev libgl1-mesa-dev lib32stdc++6
      lib32z1"
    for p in $packages; do
      check::is-package-installed $p || check::install-package $p
    done
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

  pushd $outdir >/dev/null
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
  popd >/dev/null
}

# Patches a checkout for building static standalone libs
# $1: The platform type.
# $2: The output directory.
function patch() {
  local platform="$1"
  local outdir="$2"

  pushd $outdir/src >/dev/null
  # This removes the examples from being built.
  sed -i.bak 's|"//webrtc/examples",|#"//webrtc/examples",|' BUILD.gn
  # This patches a GN error with the video_loopback executable depending on a
  # test but since we disable building tests GN detects a dependency error.
  # Replacing the outer conditional with 'rtc_include_tests' works around this.
  sed -i.bak 's|if (!build_with_chromium)|if (rtc_include_tests)|' webrtc/BUILD.gn
  popd >/dev/null
}

# This function combines build artifact objects into one library named by
# 'outputlib'.
# $1: The directory containing .ninja_deps and build artifacts.
# $2: The output library name.
function combine-objs() {
  local objs="$1"
  local outputlib="$2"
  local blacklist="unittest_main.o"

  # Combine all objects into one static library. Prevent blacklisted objects
  # such as ones containing a main function from being combined.
  echo "$objs" | grep -v -E $blacklist | xargs ar crs $outputlib
}

# This compiles the library.
# $1: The platform type.
# $2: The output directory.
function compile() {
  local platform="$1"
  local outdir="$2"
  local target_os="" # TODO: one-day support cross-compiling via options
  local target_cpu="" # TODO: also one-day support this
  local common_args="is_component_build=false rtc_include_tests=false"
  local platform_args=""

  pushd $outdir/src >/dev/null
  case $platform in
  windows)
    # do the build
    python src/webrtc/build/gyp_webrtc.py
    ninja -C src/out/Debug
    ninja -C src/out/Release

    # 64-bit build
    GYP_DEFINES="target_arch=x64 $GYP_DEFINES"

    # do the build
    python src/webrtc/build/gyp_webrtc.py
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
    # On Linux, use clang = false and sysroot = false to build using gcc.
    # Comment this out to use clang.
    if [ $platform = 'linux64' ]; then
      platform_args="is_clang=false use_sysroot=false"
    fi

    # Debug builds are component builds (shared libraries) by default unless
    # is_component_build=false is passed to gn gen --args. Release builds are
    # static by default.
    gn gen out/Debug --args="$common_args $platform_args"
    pushd out/Debug >/dev/null
      ninja -C .

      rm -f libwebrtc_full.a
      # Produce an ordered objects list by parsing .ninja_deps for strings
      # matching .o files.
      local objlist=$(strings .ninja_deps | grep -o '.*\.o')
      combine-objs "$objlist" libwebrtc_full.a

      # various intrinsics aren't included by default in .ninja_deps
      local extras=$(find \
        ./obj/third_party/libvpx/libvpx_* \
        ./obj/third_party/libjpeg_turbo/simd_asm -name *.o)
      combine-objs "$extras" libwebrtc_full.a
    popd >/dev/null

    gn gen out/Release --args="is_debug=false $common_args $platform_args"
    pushd out/Release >/dev/null
      ninja -C .

      rm -f libwebrtc_full.a
      # Produce an ordered objects list by parsing .ninja_deps for strings
      # matching .o files.
      local objlist=$(strings .ninja_deps | grep -o '.*\.o')
      combine-objs "$objlist" libwebrtc_full.a

      # various intrinsics aren't included by default in .ninja_deps
      local extras=$(find \
        ./obj/third_party/libvpx/libvpx_* \
        ./obj/third_party/libjpeg_turbo/simd_asm -name *.o)
      combine-objs "$extras" libwebrtc_full.a
    popd >/dev/null
    ;;
  esac
  popd >/dev/null
}

# This packages a compiled build into a zip file in the output directory.
# $1: The platform type.
# $2: The output directory.
# $3: Label of the package.
# $4: The project's resource dirctory.
function package() {
  local platform="$1"
  local outdir="$2"
  local label="$3"
  local resourcedir="$4"

  if [ $platform = 'osx' ]; then
    CP='gcp'
  else
    CP='cp'
  fi
  pushd $outdir >/dev/null
  # create directory structure
  mkdir -p $label/include $label/lib
  # find and copy header files
  pushd src >/dev/null
  find webrtc -name *.h -exec $CP --parents '{}' $outdir/$label/include ';'
  popd >/dev/null
  # find and copy libraries
  pushd src/out >/dev/null
  find . -maxdepth 3 \( -name *.so -o -name *webrtc_full* -o -name *.jar \) \
    -exec $CP --parents '{}' $outdir/$label/lib ';'
  popd >/dev/null

  # for linux64, add pkgconfig files
  if [ $platform = 'linux64' ]; then
    configs="Debug Release"
    for cfg in $configs; do
      mkdir -p $label/lib/$cfg/pkgconfig
      CONFIG=$cfg envsubst '$CONFIG' < $resourcedir/pkgconfig/libwebrtc_full.pc.in > \
        $label/lib/$cfg/pkgconfig/libwebrtc_full.pc
    done
  fi

  # remove first for cleaner builds
  rm -f $label.zip

  # zip up the package
  if [ $platform = 'windows' ]; then
    $DEPOT_TOOLS/win_toolchain/7z/7z.exe a -tzip $label.zip $label
  else
    zip -r $label.zip $label >/dev/null
  fi
  popd >/dev/null
}

# This returns the latest revision from the git repo.
# $1: The git repo URL
function latest-rev() {
  local repo_url="$1"
  git ls-remote $repo_url HEAD | cut -f1
}

# This returns the associated revision number for a given git sha revision
# $1: The git repo URL
# $2: The revision git sha string
function revision-number() {
  local repo_url="$1"
  local revision="$2"
  # This says curl the revision log with text format, base64 decode it using
  # openssl since its more portable than just 'base64', take the last line which
  # contains the commit revision number and output only the matching {#nnn} part
  openssl base64 -d -A <<< $(curl --silent $repo_url/+/$revision?format=TEXT) \
    | tail -1 | egrep -o '{#([0-9]+)}' | tr -d '{}#'
}

# This returns a short revision sha.
# $1: The revision string
function short-rev() {
  local revision="$1"
  echo $revision | cut -c -7
}
