#!/bin/bash
set -eo pipefail
set -x

# This goes through the entire build sequence

# win deps: git, tee
# lin deps: git, tee
# osx deps: git, tee

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/environment.sh

usage ()
{
cat << EOF

usage:
   $0 options

Build script.

OPTIONS:
   -h   Show this message
   -r   Revision represented as a git SHA (optional, builds latest revision if omitted)
EOF
}

while getopts :r: OPTION
do
   case $OPTION in
       r)
           REVISION=$OPTARG
           ;;
       ?)
           usage
           exit 1
           ;;
   esac
done

# clean first
$DIR/clean.sh 2>&1

# generate directory to build in
BUILD_DIR=$OUT_DIR

if [ -z $REVISION ]; then
  # If no revision given, then get the latest revision from git ls-remote
  REVISION=`git ls-remote $REPO_URL HEAD | cut -f1`
  if [ -z $REVISION ]; then
    echo "Could not get latest revision"
    exit 2
  fi
fi

$DIR/check_depot_tools.sh 2>&1 | tee $BUILD_DIR/check_depot_tools.log
$DIR/check_deps.sh 2>&1 | tee $BUILD_DIR/check_deps.log
$DIR/checkout.sh -r $REVISION -d $BUILD_DIR 2>&1 | tee $BUILD_DIR/checkout.log
$DIR/patch.sh -d $BUILD_DIR 2>&1 | tee $BUILD_DIR/patch.log
$DIR/compile.sh -d $BUILD_DIR 2>&1 | tee $BUILD_DIR/compile.log
$DIR/package.sh -r $REVISION -d $BUILD_DIR 2>&1 | tee $BUILD_DIR/package.log

# for extensibility
if [ -f $DIR/build.local ]; then
  $DIR/build.local -r $REVISION -d $BUILD_DIR 2>&1 | tee $BUILD_DIR/build.local.log
fi
