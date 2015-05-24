#!/bin/bash
set -eo pipefail
set -x

# This compiles a single build

# win deps: gclient, ninja
# lin deps: gclient, ninja

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/environment.sh

usage ()
{
cat << EOF

usage:
   $0 options

Compile script.

OPTIONS:
   -h   Show this message
   -d   Top level build dir
   -r   Revision represented as a git SHA
EOF
}

while getopts :d:r: OPTION
do
   case $OPTION in
       d)
           BUILD_DIR=$OPTARG
           ;;
       r)
           REVISION=$OPTARG
           ;;
       ?)
           usage
           exit 1
           ;;
   esac
done

if [ -z "$BUILD_DIR" -o -z "$REVISION" ]; then
   usage
   exit 1
fi

# gclient only works from the build directory
pushd $BUILD_DIR

# start with no tests
export GYP_DEFINES='include_tests=0'

if [ $UNAME = 'Windows_NT' ]; then
  export DEPOT_TOOLS_WIN_TOOLCHAIN=0
  gclient sync --force --revision $REVISION

  # patch for directx not found
  sed -i 's|\(#include <d3dx9.h>\)|//\1|' $BUILD_DIR/src/webrtc/modules/video_render/windows/video_render_direct3d9.h
  sed -i 's|\(D3DXMATRIX\)|//\1|' $BUILD_DIR/src/webrtc/modules/video_render/windows/video_render_direct3d9.cc

  # patch all platforms to build standalone libs
  find src/webrtc src/talk src/chromium/src/third_party \( -name *.gyp -o  -name *.gypi \) -not -path *libyuv* -exec sed -i "s|\('type': 'static_library',\)|\1 'standalone_static_library': 1,|" '{}' ';'
  # also for the libs that say 'type': '<(component)' like nss and icu
  find src/chromium/src/third_party src/net/third_party/nss \( -name *.gyp -o  -name *.gypi \) -not -path *libyuv* -exec sed -i "s|\('type': '<(component)',\)|\1 'standalone_static_library': 1,|" '{}' ';'
  cmd //c "$WIN_DEPOT_TOOLS\gclient.bat runhooks"
  
  # do the build
  ninja -C src/out/Debug
  ninja -C src/out/Release

  # 64-bit build
  export GYP_DEFINES="target_arch=x64 $GYP_DEFINES"
  cmd //c "$WIN_DEPOT_TOOLS\gclient.bat runhooks"

  # do the build
  ninja -C src/out/Debug_x64
  ninja -C src/out/Release_x64

  # combine all the static libraries into one called webrtc_full
  # LIB.EXE /OUT:c.lib a.lib b.lib
  "$VS120COMNTOOLS../../VC/bin/lib" /OUT:src/out/Debug/webrtc_full.lib src/out/Debug/*.lib
  "$VS120COMNTOOLS../../VC/bin/lib" /OUT:src/out/Release/webrtc_full.lib src/out/Release/*.lib
  "$VS120COMNTOOLS../../VC/bin/lib" /OUT:src/out/Debug_x64/webrtc_full.lib src/out/Debug_x64/*.lib
  "$VS120COMNTOOLS../../VC/bin/lib" /OUT:src/out/Release_x64/webrtc_full.lib src/out/Release_x64/*.lib 
else
  # linux and osx
  
  gclient sync --force --revision $REVISION

  # patch all platforms to build standalone libs
  find src/webrtc src/talk src/chromium/src/third_party \( -name *.gyp -o  -name *.gypi \) -not -path *libyuv* -exec sed -i "s|\('type': 'static_library',\)|\1 'standalone_static_library': 1,|" '{}' ';'
  # for icu only; icu_use_data_file_flag is 1 on linux
  find src/chromium/src/third_party/icu/icu.gyp \( -name *.gyp -o  -name *.gypi \) -exec sed -i "s|\('type': 'none',\)|\1 'standalone_static_library': 0,|" '{}' ';'
  gclient runhooks
  
  # do the build
  ninja -C src/out/Debug
  ninja -C src/out/Release

  # combine all the static libraries into one called webrtc_full
  pushd src/out/Debug
  find . -name '*.a' -exec ar -x '{}' ';'
  ar -crs libwebrtc_full.a *.o
  rm *.o
  popd

  pushd src/out/Release
  find . -name '*.a' -exec ar -x '{}' ';'
  ar -crs libwebrtc_full.a *.o
  rm *.o
  popd
fi

popd
