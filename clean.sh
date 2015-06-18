#!/bin/bash
set -eo pipefail
set -x

# This cleans up all the builds in the output directory

# win deps: rmdir, rm
# lin deps: rm
# osx deps: rm

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/environment.sh

if [ $UNAME = 'Windows' ]; then
  pushd $OUT_DIR
  # windows rmdir
  cmd //c "for /D %f in (*) do rmdir /s /q %f" || true
  # and again, for any stragglers
  rm -rf * .gclient*
  popd
else
  rm -rf $OUT_DIR/* $OUT_DIR/.gclient*
fi
