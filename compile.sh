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

# gclient only works from the build directory
pushd $BUILD_DIR

# start with no tests
export GYP_DEFINES='build_with_chromium=0 include_tests=0 disable_glibcxx_debug=1 linux_use_debug_fission=0'

if [ $UNAME = 'Windows' ]; then
  export DEPOT_TOOLS_WIN_TOOLCHAIN=0

  # do the build
  cmd //c "$WIN_DEPOT_TOOLS\gclient.bat runhooks"
  ninja -C src/out/Debug
  ninja -C src/out/Release

  # 64-bit build
  export GYP_DEFINES="target_arch=x64 $GYP_DEFINES"

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
else
  # linux and osx

  # for android, which gets built on linux
  if [ $PLATFORM = 'android' ]; then
    export GYP_DEFINES="OS=android $GYP_DEFINES"
    . src/build/android/envsetup.sh
  elif [ $PLATFORM = 'osx' ]; then
    export GYP_DEFINES="target_arch=x64 $GYP_DEFINES"
  fi

  # do the build
  configs=( "Debug" "Release" )
  for c in "${configs[@]}"; do
    gclient runhooks
    # ninja uses all cores. can optionally take a -j argument via USE_JOBS_OVERRIDE
    ninja $USE_JOBS_OVERRIDE -C src/out/$c

    # combine all the static libraries into one called webrtc_full
    pushd src/out/$c
    find . -name '*.a' -exec ar -x '{}' ';'
    ar -crs libwebrtc_full.a *.o
    rm *.o
    popd
  done
fi

popd
