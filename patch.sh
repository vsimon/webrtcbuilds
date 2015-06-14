#!/bin/bash
set -eo pipefail
set -x

# This patches a checkout

# win deps: find, sed
# lin deps: find, sed
# osx deps: find, gsed

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/environment.sh

usage ()
{
cat << EOF

usage:
   $0 options

Patch script.

OPTIONS:
   -h   Show this message
   -d   Top level build dir
EOF
}

while getopts :d: OPTION
do
   case $OPTION in
       d)
           BUILD_DIR=$OPTARG
           ;;
       ?)
           usage
           exit 1
           ;;
   esac
done

if [ -z "$BUILD_DIR" ]; then
   usage
   exit 1
fi

pushd $BUILD_DIR

if [ $UNAME = 'Windows' ]; then
  # patch for directx not found
  sed -i 's|\(#include <d3dx9.h>\)|//\1|' $BUILD_DIR/src/webrtc/modules/video_render/windows/video_render_direct3d9.h
  sed -i 's|\(D3DXMATRIX\)|//\1|' $BUILD_DIR/src/webrtc/modules/video_render/windows/video_render_direct3d9.cc

  # patch all platforms to build standalone libs
  find src/webrtc src/talk src/chromium/src/third_party \( -name *.gyp -o  -name *.gypi \) -not -path *libyuv* -exec sed -i "s|\('type': 'static_library',\)|\1 'standalone_static_library': 1,|" '{}' ';'
  # also for the libs that say 'type': '<(component)' like nss and icu
  find src/chromium/src/third_party src/net/third_party/nss \( -name *.gyp -o  -name *.gypi \) -not -path *libyuv* -exec sed -i "s|\('type': '<(component)',\)|\1 'standalone_static_library': 1,|" '{}' ';'
else
  # linux and osx

  if [ $PLATFORM = 'android' ]; then
    echo "nothing to patch"
  else
    # sed
    if [ $UNAME = 'Darwin' ]; then
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
  fi
fi

popd
