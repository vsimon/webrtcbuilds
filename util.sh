# Detect host platform.
# Set PLATFORM environment variable to override default behavior.
# Supported platform types - 'linux', 'win', 'mac'
# 'msys' is the git bash shell, built using mingw-w64, running under Microsoft
# Windows.
function detect-platform() {
  # set PLATFORM to android on linux host to build android
  case "$OSTYPE" in
  darwin*)      PLATFORM=${PLATFORM:-mac} ;;
  linux*)       PLATFORM=${PLATFORM:-linux} ;;
  win32*|msys*) PLATFORM=${PLATFORM:-win} ;;
  *)            echo "Building on unsupported OS: $OSTYPE"; exit 1; ;;
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
    if [ $platform = 'win' ]; then
      # run gclient.bat to get python
      pushd $depot_tools_dir >/dev/null
      ./gclient.bat
      popd >/dev/null
    fi
  else
    pushd $depot_tools_dir >/dev/null
      git reset --hard -q
    popd >/dev/null
  fi
}

# Makes sure package is installed. Depends on sudo to be installed first.
# $1: The name of the package
# $2: Existence check binary. Defaults to name of the package.
function ensure-package() {
  local name="$1"
  local binary="${2:-$1}"
  if ! which $binary > /dev/null ; then
    sudo apt-get update -qq
    sudo apt-get install -y $name
  fi
}

# Makes sure all webrtcbuilds dependencies are present.
# $1: The platform type.
function check::webrtcbuilds::deps() {
  local platform="$1"

  case $platform in
  mac)
    which brew > /dev/null || (echo "Building with 'mac' requires Homebrew. Visit https://brew.sh"; exit 1)
    # for GNU version of cp: gcp
    which gcp > /dev/null || brew install coreutils
    ;;
  linux)
    if ! grep -v \# /etc/apt/sources.list | grep -q multiverse ; then
      echo "*** Warning: The Multiverse repository is probably not enabled ***"
      echo "*** which is required for things like msttcorefonts.           ***"
    fi
    if ! which sudo > /dev/null ; then
      apt-get update -qq
      apt-get install -y sudo
    fi
    ensure-package curl
    ensure-package git
    ensure-package python
    ensure-package lbzip2
    ensure-package lsb-release lsb_release
    ;;
  win)
    VISUAL_STUDIO_TOOLS=${VS140COMNTOOLS:-}
    if [ -z VISUAL_STUDIO_TOOLS ]; then
      echo "Building with 'win' requires Microsoft Visual Studio 2015"
      exit 1
    fi
;;
  esac
}

# Makes sure all WebRTC build dependencies are present.
# $1: The platform type.
function check::webrtc::deps() {
  local platform="$1"
  local outdir="$2"
  local target_os="$3"

  case $platform in
  linux)
    # Automatically accepts ttf-mscorefonts EULA
    echo ttf-mscorefonts-installer msttcorefonts/accepted-mscorefonts-eula select true | sudo debconf-set-selections
    sudo $outdir/src/build/install-build-deps.sh --no-syms --no-arm --no-chromeos-fonts --no-nacl --no-prompt
    ;;
  esac

  if [ $target_os = 'android' ]; then
    sudo $outdir/src/build/install-build-deps-android.sh
  fi
}

# Checks out a specific revision
# $1: The target OS type.
# $2: The output directory.
# $3: Revision represented as a git SHA.
function checkout() {
  local target_os="$1"
  local outdir="$2"
  local revision="$3"

  pushd $outdir >/dev/null
  local prev_target_os=$(cat $outdir/.webrtcbuilds_target_os 2>/dev/null)
  if [[ -n "$prev_target_os" && "$target_os" != "$prev_target_os" ]]; then
    echo The target OS has changed. Refetching sources for the new target OS
    rm -rf src .gclient*
  fi
  # Fetch only the first-time, otherwise sync.
  if [ ! -d src ]; then
    case $target_os in
    android)
      yes | fetch --nohooks webrtc_android
      ;;
    ios)
      fetch --nohooks webrtc_ios
      ;;
    *)
      fetch --nohooks webrtc
      ;;
    esac
  fi
  # Checkout the specific revision after fetch or for older branches, run
  # setup_links.py to replace all directories which now must be symlinks then
  # try again.
  gclient sync --force --revision $revision ||
    (test -f src/setup_links.py && src/setup_links.py --force --no-prompt && gclient sync --force --revision $revision)
  # Cache the target OS
  echo $target_os > $outdir/.webrtcbuilds_target_os
  popd >/dev/null
}

# Patches a checkout for building static standalone libs
# $1: The platform type.
# $2: The output directory.
# $3: Enable RTTI or not. '0' or '1'.
function patch() {
  local platform="$1"
  local outdir="$2"
  local rtti_enabled="$3"

  pushd $outdir/src >/dev/null
  # This removes the examples from being built.
  sed -i.bak 's|"//webrtc/examples",|#"//webrtc/examples",|' BUILD.gn
  # This configures whether to build with RTTI enabled or not.
  [ "$rtti_enabled" = 1 ] && sed -i.bak \
    's|"//build/config/compiler:no_rtti",|"//build/config/compiler:rtti",|' \
    build/config/BUILDCONFIG.gn
  popd >/dev/null
}

# This function compiles a single library using Microsoft Visual C++ for a
# Microsoft Windows (32/64-bit) target. This function is separate from the
# other compile functions because of differences using the Microsoft tools:
#
# The Microsoft Windows tools use different file extensions than the other tools:
#  '.obj' as the object file extension, instead of '.o'
# '.lib' as the static library file extension, instead of '.a'
# '.dll' as the shared library file extension, instead of '.so'
#
# The Microsoft Windows tools have different names than the other tools:
# 'lib' as the librarian, instead of 'ar'. 'lib' must be found through the path
# variable $VS140COMNTOOLS.
#
# The Microsoft tools that target Microsoft Windows run only under
# Microsoft Windows, so the build and target systems are the same.
#
# $1 the output directory, 'Debug', 'Debug_x64', 'Release', or 'Release_x64'
# $2 additional gn arguments
function compile-win() {
  local outputdir="$1"
  local gn_args="$2"

  gn gen $outputdir --args="$gn_args"
  pushd $outputdir >/dev/null
  ninja -C .

  rm -f libwebrtc_full.lib
  local objlist=$(strings .ninja_deps | grep -o '.*\.obj')
  local blacklist="unittest_main.obj|video_capture_external.obj|\
device_info_external.obj"
  echo "$objlist" | tr ' ' '\n' | grep -v -E $blacklist >libwebrtc_full.list
  # On Windows, asm files are compiled to object files that have .obj and .o
  # extensions.
  local extras=$(find \
    ./obj/third_party/libvpx/libvpx_* \
    ./obj/third_party/libjpeg_turbo/simd_asm \
    ./obj/third_party/boringssl/boringssl_asm -name '*.o' -o -name '*.obj')
  echo "$extras" | tr ' ' '\n' >>libwebrtc_full.list
  "$VS140COMNTOOLS../../VC/bin/lib" /OUT:webrtc_full.lib @libwebrtc_full.list
  popd >/dev/null
}

# This function compile and combine build artifact objects into one library.
# $1 the output directory, 'Debug', 'Release'
# $2 additional gn arguments
function compile-unix() {
  local outputdir="$1"
  local gn_args="$2"
  local blacklist="unittest|examples|/yasm|protobuf_lite|main.o|\
video_capture_external.o|device_info_external.o"

  gn gen $outputdir --args="$gn_args"
  pushd $outputdir >/dev/null
  ninja -C .

  rm -f libwebrtc_full.a
  # Produce an ordered objects list by parsing .ninja_deps for strings
  # matching .o files.
  local objlist=$(strings .ninja_deps | grep -o '.*\.o')
  echo "$objlist" | tr ' ' '\n' | grep -v -E $blacklist >libwebrtc_full.list
  # various intrinsics aren't included by default in .ninja_deps
  local extras=$(find \
    ./obj/third_party/libvpx/libvpx_* \
    ./obj/third_party/libjpeg_turbo/simd_asm \
    ./obj/third_party/boringssl/boringssl_asm -name '*.o')
  echo "$extras" | tr ' ' '\n' >>libwebrtc_full.list
  # generate the archive
  cat libwebrtc_full.list | xargs ar -crs libwebrtc_full.a
  # generate an index list
  ranlib libwebrtc_full.a
  popd >/dev/null
}

# This compiles the library.
# $1: The platform type.
# $2: The output directory.
# $3: The target os for cross-compilation.
# $4: The target cpu for cross-compilation.
# $5: The build configurations.
# $6: Enable iterator debug or not. '0' or '1'.
function compile() {
  local platform="$1"
  local outdir="$2"
  local target_os="$3"
  local target_cpu="$4"
  local configs="$5"
  local disable_iterator_debug="$6"
  local common_args="is_component_build=false rtc_include_tests=false treat_warnings_as_errors=false"
  local target_args="target_os=\"$target_os\" target_cpu=\"$target_cpu\""

  [ "$disable_iterator_debug" = 1 ] && common_args+=' enable_iterator_debugging=false'
  pushd $outdir/src >/dev/null
  for cfg in $configs; do
    [ "$cfg" = 'Release' ] && common_args+=' is_debug=false'
    case $platform in
    win)
      # 32-bit build
      compile-win "out/$cfg" "$common_args $target_args"

      # 64-bit build
      GYP_DEFINES="target_arch=x64 $GYP_DEFINES"
      compile-win "out/${cfg}_x64" "$common_args $target_args"
      ;;
    *)
      # On Linux when not cross-compiling, use clang = false and sysroot = false
      # to build using gcc. Comment this out to use clang.
      if [[ $platform = 'linux' && $target_cpu != amd* ]]; then
        target_args+=" is_clang=false use_sysroot=false"
      fi

      # Debug builds are component builds (shared libraries) by default unless
      # is_component_build=false is passed to gn gen --args. Release builds are
      # static by default.
      compile-unix "out/$cfg" "$common_args $target_args"
      ;;
    esac
  done
  popd >/dev/null
}

# This prepares artifacts to be packaged.
# $1: The platform type.
# $2: The output directory.
# $3: The package filename.
# $4: The project's resource dirctory.
# $5: The build configurations.
# $6: : The revision number.
function package::prepare() {
  local platform="$1"
  local outdir="$2"
  local package_filename="$3"
  local resourcedir="$4"
  local configs="$5"
  local revision_number="$6"

  if [ $platform = 'mac' ]; then
    CP='gcp'
  else
    CP='cp'
  fi
  pushd $outdir >/dev/null
  # create directory structure
  mkdir -p $package_filename/include $package_filename/lib
  for cfg in $configs; do
    mkdir -p $package_filename/lib/$cfg
  done

  # find and copy header files
  pushd src >/dev/null
  local headersSourceDir=webrtc
  local headersDestDir=$outdir/$package_filename/include
  # Revision 19846 is the following, where upstream moved src/webrtc to src/
  # https://webrtc.googlesource.com/src/+/92ea95e34af5966555903026f45164afbd7e2088
  if [[ $revision_number -ge 19846 ]]; then
    headersSourceDir=.
  fi
  find $headersSourceDir -name '*.h' -exec $CP --parents '{}' $headersDestDir ';'
  popd >/dev/null
  # find and copy libraries
  pushd src/out >/dev/null
  find . -maxdepth 3 \( -name '*.so' -o -name '*.dll' -o -name '*webrtc_full*' -o -name *.jar \) \
    -exec $CP --parents '{}' $outdir/$package_filename/lib ';'
  popd >/dev/null

  # for linux, add pkgconfig files
  if [ $platform = 'linux' ]; then
    for cfg in $configs; do
      mkdir -p $package_filename/lib/$cfg/pkgconfig
      CONFIG=$cfg envsubst '$CONFIG' < $resourcedir/pkgconfig/libwebrtc_full.pc.in > \
        $package_filename/lib/$cfg/pkgconfig/libwebrtc_full.pc
    done
  fi
  popd >/dev/null
}

# This packages a compiled build into a zip file in the output directory.
# $1: The platform type.
# $2: The output directory.
# $3: The package filename.
function package::zip() {
  local platform="$1"
  local outdir="$2"
  local package_filename="$3"

  pushd $outdir >/dev/null
  # remove first for cleaner builds
  rm -f $package_filename.zip

  # zip up the package
  if [ $platform = 'win' ]; then
    $DEPOT_TOOLS/win_toolchain/7z/7z.exe a -tzip $package_filename.zip $package_filename
  else
    zip -r $package_filename.zip $package_filename >/dev/null
  fi
  popd >/dev/null
}

# This packages into a debian package in the output directory.
# $1: The output directory.
# $2: The package filename.
# $3: The package name.
# $4: The package version.
# $5: The architecture.
function package::debian() {
  local outdir="$1"
  local package_filename="$2"
  local package_name="$3"
  local package_version="$4"
  local arch="$5"
  local debianize="debianize/$package_filename"

  echo "Debianize WebRTC"
  pushd $outdir >/dev/null
  mkdir -p $debianize/DEBIAN
  mkdir -p $debianize/opt
  mv $package_filename $debianize/opt/webrtc
  cat << EOF > $debianize/DEBIAN/control
Package: $package_name
Architecture: $arch
Maintainer: webrtcbuilds manintainers
Depends: debconf (>= 0.5.00)
Priority: optional
Version: $package_version
Description: webrtc static library
 This package provides webrtc library generated with webrtcbuilds
EOF
  fakeroot dpkg-deb --build $debianize
  mv debianize/*.deb .
  rm -rf debianize
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

# This returns a short revision sha.
# $1: The target cpu for cross-compilation.
function debian-arch() {
  local target_cpu="$1"
  # set PLATFORM to android on linux host to build android
  case "$target_cpu" in
  x86*)         echo "i386" ;;
  x64*)         echo "amd64" ;;
  *)            echo "$target_cpu" ;;
  esac
}

# This interprets a pattern and returns the interpreted one.
# $1: The pattern.
# $2: The output directory.
# $3: The platform type.
# $4: The target os for cross-compilation.
# $5: The target cpu for cross-compilation.
# $6: The branch.
# $7: The revision.
# $8: The revision number.
function interpret-pattern() {
  local pattern="$1"
  local platform="$2"
  local outdir="$3"
  local target_os="$4"
  local target_cpu="$5"
  local branch="$6"
  local revision="$7"
  local revision_number="$8"
  local debian_arch="$(debian-arch $target_cpu)"
  local short_revision="$(short-rev $revision)"

  pattern=${pattern//%p%/$platform}
  pattern=${pattern//%to%/$target_os}
  pattern=${pattern//%tc%/$target_cpu}
  pattern=${pattern//%b%/$branch}
  pattern=${pattern//%r%/$revision}
  pattern=${pattern//%rn%/$revision_number}
  pattern=${pattern//%da%/$debian_arch}
  pattern=${pattern//%sr%/$short_revision}

  echo "$pattern"
}
