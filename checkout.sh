#!/bin/bash
set -eo pipefail
set -x

# This checks out a specific revision

# win deps: depot_tools
# lin deps: depot_tools
# osx deps: depot_tools

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
   -r   Revision represented as a git SHA (optional, checks out latest revision if omitted)
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

if [ -z "$BUILD_DIR" ]; then
   usage
   exit 1
fi

if [ -z $REVISION ]; then
  # If no revision given, then get the latest revision from git ls-remote
  REVISION=`git ls-remote $REPO_URL HEAD | cut -f1`
fi

# gclient only works from the build directory
pushd $BUILD_DIR

if [ -z $USE_PICKLE ]; then
  # first fetch
  if [ $PLATFORM = 'android' ]; then
    fetch webrtc_android
  else
    fetch webrtc
  fi
else
  curl -L -O --silent https://github.com/vsimon/webrtcbuilds-builder/releases/download/pickle/pickle.tar.gz.0[0-5]
  cat pickle.tar.gz.* | tar xzpf -
fi

# check out the specific revision after fetch
gclient sync --force --revision $REVISION

popd
