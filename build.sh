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
EOF
}

while getopts :b:o:r:t:c:n:degD OPTION; do
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
  ?) usage; exit 1 ;;
  esac
done

OUTDIR=${OUTDIR:-out}
BRANCH=${BRANCH:-}
DEBUG=${DEBUG:-0}
ENABLE_RTTI=${ENABLE_RTTI:-0}
DISABLE_ITERATOR_DEBUG=${DISABLE_ITERATOR_DEBUG:-0}
PACKAGE_AS_DEBIAN=${PACKAGE_AS_DEBIAN:-0}
CONFIGS=${CONFIGS:-Debug Release}
PROJECT_NAME=webrtcbuilds
REPO_URL="https://chromium.googlesource.com/external/webrtc"
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

echo Checking webrtcbuilds dependencies
check::webrtcbuilds::deps $PLATFORM

echo Checking depot-tools
check::depot-tools $PLATFORM $DEPOT_TOOLS_URL $DEPOT_TOOLS_DIR

if [ ! -z $BRANCH ]; then
  REVISION=$(git ls-remote $REPO_URL --heads $BRANCH | head --lines 1 | cut --fields 1) || \
    { echo "Cound not get branch revision" && exit 1; }
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
# label is <projectname>-<rev-number>-<short-rev-sha>-<target-os>-<target-cpu>
LABEL=$PROJECT_NAME-$REVISION_NUMBER-$(short-rev $REVISION)-$TARGET_OS-$TARGET_CPU
package $PLATFORM $OUTDIR $LABEL $DIR/resource "$CONFIGS"
[ "$PACKAGE_AS_DEBIAN" = 1 ] && package::debian $OUTDIR $LABEL $REVISION_NUMBER

echo Build successful
