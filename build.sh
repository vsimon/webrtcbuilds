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
   -m MSVSVER    Microsoft Visual Studio (C++) version (e.g. 2013). Default is Chromium build default.
   -o OUTDIR     Output directory. Default is 'out'
   -r REVISION   Git SHA revision. Default is latest revision.
EOF
}

while getopts :m:o:r: OPTION; do
  case $OPTION in
  m) MSVSVER=$OPTARG ;;
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

# If a Microsoft Visual Studio (C++) version is given, override the Chromium build default.
MSVSVER=${MSVSVER:-}
if [ -n "$MSVSVER" ]; then
  export GYP_MSVS_VERSION=$MSVSVER
fi

set-platform
clean $OUTDIR
check::deps $PLATFORM
check::depot-tools $PLATFORM $DEPOT_TOOLS_URL $DEPOT_TOOLS_DIR

# If no revision given, then get the latest revision from git ls-remote
REVISION=${REVISION:-$(git ls-remote $REPO_URL HEAD | cut -f1)} || \
  { echo "Could not get latest revision" && exit 1; }
echo "Building revision: $REVISION"

checkout $PLATFORM $OUTDIR $REVISION
patch $PLATFORM $OUTDIR
compile $PLATFORM $OUTDIR
package $PLATFORM $OUTDIR $REVISION $DIR/resource
