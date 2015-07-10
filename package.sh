#!/bin/bash
set -eo pipefail
set -x

# This packages a completed build resulting in a zip file in the build directory

# win deps: sed, 7z
# lin deps: sed, zip
# osx deps: gsed, zip

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/environment.sh

usage ()
{
cat << EOF

usage:
   $0 options

Package script.

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

pushd $BUILD_DIR

# quick-check if something has built
if [ ! -d src/out ]; then
  popd
  echo "nothing to package"
  exit 2
fi

# go into the webrtc repo to get the revision number from git log
pushd src
# sed
if [ $UNAME = 'Darwin' ]; then
  SED='gsed'
else
  SED='sed'
fi
REVISION_NUMBER=`git log -1 | tail -1 | $SED -ne 's|.*@\W*\([0-9]\+\).*$|\1|p'`
if [ -z $REVISION_NUMBER ]; then
  echo "Could not get revision number for packaging"
  exit 3
fi
REVISION_SHORT=`git rev-parse --short $REVISION`
popd

# create a build label
BUILDLABEL=$PROJECT_NAME-$REVISION_NUMBER-$REVISION_SHORT-$PLATFORM

if [ $UNAME = 'Darwin' ]; then
  CP="gcp"
else
  CP="cp"
fi

# create directory structure
mkdir -p $BUILDLABEL/bin $BUILDLABEL/include $BUILDLABEL/lib

# find and copy everything that is not a library into bin
find src/out/Release -maxdepth 1 -type f \
  -not -name *.so -not -name *.a -not -name *.jar -not -name *.lib \
  -not -name *.isolated \
  -not -name *.state \
  -not -name *.ninja \
  -not -name *.tmp \
  -not -name *.pdb \
  -not -name *.res \
  -not -name *.rc \
  -not -name *.x64 \
  -not -name *.x86 \
  -not -name *.ilk \
  -not -name *.TOC \
  -not -name gyp-win-tool \
  -not -name *.manifest \
  -not -name \\.* \
  -exec $CP '{}' $BUILDLABEL/bin ';'

# find and copy header files
find src/webrtc src/talk src/chromium/src/third_party/jsoncpp -name *.h \
  -exec $CP --parents '{}' $BUILDLABEL/include ';'
mv $BUILDLABEL/include/src/* $BUILDLABEL/include
mv $BUILDLABEL/include/chromium/src/third_party/jsoncpp/source/include/* $BUILDLABEL/include
rm -rf $BUILDLABEL/include/src $BUILDLABEL/include/chromium

# find and copy libraries
find src/out -maxdepth 3 \( -name *.so -o -name *webrtc_full* -o -name *.jar \) \
  -exec $CP --parents '{}' $BUILDLABEL/lib ';'
mv $BUILDLABEL/lib/src/out/* $BUILDLABEL/lib
rmdir $BUILDLABEL/lib/src/out $BUILDLABEL/lib/src

# for linux64, add pkgconfig files
if [ $UNAME = "Linux" ]; then
  mkdir -p $BUILDLABEL/lib/Debug/pkgconfig $BUILDLABEL/lib/Release/pkgconfig
  $CP $DIR/resources/pkgconfig/libwebrtc_full-debug.pc $BUILDLABEL/lib/Debug/pkgconfig/libwebrtc_full.pc
  $CP $DIR/resources/pkgconfig/libwebrtc_full-release.pc $BUILDLABEL/lib/Release/pkgconfig/libwebrtc_full.pc
fi

# zip up the package
if [ $UNAME = 'Windows' ]; then
  $DEPOT_TOOLS/win_toolchain/7z/7z.exe a -tzip $BUILDLABEL.zip $BUILDLABEL
else
  zip -r $BUILDLABEL.zip $BUILDLABEL
fi

# archive revision_number
echo $REVISION_NUMBER > revision_number

popd
