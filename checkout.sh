#!/bin/bash
set -eo pipefail
set -x

# This checks out a specific revision

# win req: depot_tools
# lin req: depot_tools
# osx req: depot_tools

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/environment.sh

usage ()
{
cat << EOF

usage:
   $0 options

Checkout script.

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

fetch webrtc
gclient sync --force --revision $REVISION

popd
