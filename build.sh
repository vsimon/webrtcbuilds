#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/util.sh

usage ()
{
cat << EOF

Usage:
   $0 [OPTIONS]

WebRTC build script.

OPTIONS:
   -h             Show this message
   -d             Debug mode. Print all executed commands.
   -o OUTDIR      Output directory. Default is 'out'
   -b BRANCH      Latest revision on git branch. Overrides -r. Common branch names are 'branch-heads/nn', where 'nn' is the release number.
   -r REVISION    Git SHA revision. Default is latest revision.
   -t TARGET OS   The target os for cross-compilation. Default is the host OS such as 'linux', 'mac', 'win'. Other values can be 'android', 'ios'.
   -c TARGET CPU  The target cpu for cross-compilation. Default is 'x64'. Other values can be 'x86', 'arm64', 'arm'.
   -n CONFIGS     Build configurations, space-separated. Default is 'Debug Release'. Other values can be 'Debug', 'Release'.
   -e             Compile WebRTC with RTTI enabled. Default is with RTTI not enabled.
   -g             [Linux] Compile 'Debug' WebRTC with iterator debugging disabled. Default is enabled but it might add significant overhead.
   -D             [Linux] Generate a debian package
   -F PATTERN     Allow customize package filename through a pattern
   -P PATTERN     Allow customize package name through a pattern
   -V PATTERN     Allow customize package version through a pattern

The PATTERN is a string that can use the following tokens:
   %p%            The system platform.
   %to%           Target os.
   %tc%           Target cpu.
   %b%            The branch if it was specified.
   %r%            Revision.
   %sr%           Short revision.
   %rn%           The associated revision number.
   %da%           Debian architecture.
EOF
}

while getopts :b:o:r:t:c:n:degDF:P:V: OPTION; do
  case $OPTION in
  o) OUTDIR=$OPTARG ;;
  b) BRANCH=$OPTARG ;;
  r) REVISION=$OPTARG ;;
  t) TARGET_OS=$OPTARG ;;
  c) TARGET_CPU=$OPTARG ;;
  n) CONFIGS=$OPTARG ;;
  d) DEBUG=1 ;;
  e) ENABLE_RTTI=1 ;;
  g) DISABLE_ITERATOR_DEBUG=1 ;;
  D) PACKAGE_AS_DEBIAN=1 ;;
  F) PACKAGE_FILENAME_PATTERN=$OPTARG ;;
  P) PACKAGE_NAME_PATTERN=$OPTARG ;;
  V) PACKAGE_VERSION_PATTERN=$OPTARG ;;
  ?) usage; exit 1 ;;
  esac
done

OUTDIR=${OUTDIR:-out}
BRANCH=${BRANCH:-}
DEBUG=${DEBUG:-0}
ENABLE_RTTI=${ENABLE_RTTI:-0}
DISABLE_ITERATOR_DEBUG=${DISABLE_ITERATOR_DEBUG:-0}
PACKAGE_AS_DEBIAN=${PACKAGE_AS_DEBIAN:-0}
PACKAGE_FILENAME_PATTERN=${PACKAGE_FILENAME_PATTERN:-"webrtcbuilds-%rn%-%sr%-%to%-%tc%"}
PACKAGE_NAME_PATTERN=${PACKAGE_NAME_PATTERN:-"webrtcbuilds"}
PACKAGE_VERSION_PATTERN=${PACKAGE_VERSION_PATTERN:-"%rn%"}
CONFIGS=${CONFIGS:-Debug Release}
REPO_URL="https://webrtc.googlesource.com/src"
DEPOT_TOOLS_URL="https://chromium.googlesource.com/chromium/tools/depot_tools.git"
DEPOT_TOOLS_DIR=$DIR/depot_tools
DEPOT_TOOLS_WIN_TOOLCHAIN=0
PATH=$DEPOT_TOOLS_DIR:$DEPOT_TOOLS_DIR/python276_bin:$PATH

[ "$DEBUG" = 1 ] && set -x

mkdir -p $OUTDIR
OUTDIR=$(cd $OUTDIR && pwd -P)

detect-platform
TARGET_OS=${TARGET_OS:-$PLATFORM}
TARGET_CPU=${TARGET_CPU:-x64}
echo "Host OS: $PLATFORM"
echo "Target OS: $TARGET_OS"
echo "Target CPU: $TARGET_CPU"

check::platform $PLATFORM $TARGET_OS

echo Checking webrtcbuilds dependencies
check::webrtcbuilds::deps $PLATFORM

echo Checking depot-tools
check::depot-tools $PLATFORM $DEPOT_TOOLS_URL $DEPOT_TOOLS_DIR

if [ ! -z $BRANCH ]; then
  REVISION=$(git ls-remote $REPO_URL --heads $BRANCH | head -n1 | cut -f1) || \
    { echo "Cound not get branch revision for $BRANCH" && exit 1; }
  [ -z $REVISION ] && echo "Cound not get branch revision for $BRANCH" && exit 1
   echo "Building branch: $BRANCH"
else
  REVISION=${REVISION:-$(latest-rev $REPO_URL)} || \
    { echo "Could not get latest revision" && exit 1; }
fi
echo "Building revision: $REVISION"
REVISION_NUMBER=$(revision-number $REPO_URL $REVISION) || \
  { echo "Could not get revision number" && exit 1; }
echo "Associated revision number: $REVISION_NUMBER"

echo "Checking out WebRTC revision (this will take awhile): $REVISION"
checkout "$TARGET_OS" $OUTDIR $REVISION

echo Checking WebRTC dependencies
check::webrtc::deps $PLATFORM $OUTDIR "$TARGET_OS"

echo Patching WebRTC source
patch $PLATFORM $OUTDIR $ENABLE_RTTI

echo Compiling WebRTC
compile $PLATFORM $OUTDIR "$TARGET_OS" "$TARGET_CPU" "$CONFIGS" "$DISABLE_ITERATOR_DEBUG"

echo Packaging WebRTC
PACKAGE_FILENAME=$(interpret-pattern "$PACKAGE_FILENAME_PATTERN" "$PLATFORM" "$OUTDIR" "$TARGET_OS" "$TARGET_CPU" "$BRANCH" "$REVISION" "$REVISION_NUMBER")
PACKAGE_NAME=$(interpret-pattern "$PACKAGE_NAME_PATTERN" "$PLATFORM" "$OUTDIR" "$TARGET_OS" "$TARGET_CPU" "$BRANCH" "$REVISION" "$REVISION_NUMBER")
PACKAGE_VERSION=$(interpret-pattern "$PACKAGE_VERSION_PATTERN" "$PLATFORM" "$OUTDIR" "$TARGET_OS" "$TARGET_CPU" "$BRANCH" "$REVISION" "$REVISION_NUMBER")
package::prepare $PLATFORM $OUTDIR $PACKAGE_FILENAME $DIR/resource "$CONFIGS" $REVISION_NUMBER
if [ "$PACKAGE_AS_DEBIAN" = 1 ]; then
  package::debian $OUTDIR $PACKAGE_FILENAME $PACKAGE_NAME $PACKAGE_VERSION "$(debian-arch $TARGET_CPU)"
else
  package::zip $PLATFORM $OUTDIR $PACKAGE_FILENAME
fi

echo Build successful
