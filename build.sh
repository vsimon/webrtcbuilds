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
   -h            Show this message
   -o OUTDIR     Output directory. Default is 'out'
   -r REVISION   Git SHA revision. Default is latest revision.
EOF
}

while getopts :o:r: OPTION; do
  case $OPTION in
  o) OUTDIR=$OPTARG ;;
  r) REVISION=$OPTARG ;;
  ?) usage; exit 1 ;;
  esac
done

OUTDIR=${OUTDIR:-out}
PROJECT_NAME=webrtcbuilds
REPO_URL="https://chromium.googlesource.com/external/webrtc"
DEPOT_TOOLS_URL="https://chromium.googlesource.com/chromium/tools/depot_tools.git"
DEPOT_TOOLS_DIR=$DIR/depot_tools
DEPOT_TOOLS_WIN_TOOLCHAIN=0
PATH=$DEPOT_TOOLS_DIR:$DEPOT_TOOLS_DIR/python276_bin:$PATH

mkdir -p $OUTDIR
OUTDIR=$(readlink -f $OUTDIR)

# This is a hack to force use of Microsoft Visual C++ 2013.
# More sophisticated code would look first for Visual C++ 2015, then 2013.
export GYP_MSVS_VERSION='2013'
echo GYP_MSVS_VERSION $GYP_MSVS_VERSION

set-platform
clean $OUTDIR
check::depot-tools $PLATFORM $DEPOT_TOOLS_URL $DEPOT_TOOLS_DIR
check::deps $PLATFORM $DEPOT_TOOLS_DIR

# If no revision given, then get the latest revision from git ls-remote
REVISION=${REVISION:-$(git ls-remote $REPO_URL HEAD | cut -f1)} || \
  { echo "Could not get latest revision" && exit 1; }
echo "Building revision: $REVISION"

checkout $PLATFORM $OUTDIR $REVISION
patch $PLATFORM $OUTDIR
compile $PLATFORM $OUTDIR
package $PLATFORM $OUTDIR $REVISION $DIR/resource
