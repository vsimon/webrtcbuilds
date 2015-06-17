#!/bin/bash
set -eo pipefail
set -x

# This checks and installs depot_tools

# win deps: git
# osx deps: git
# lin deps: git

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/environment.sh

if [ ! -d $DEPOT_TOOLS ]; then
  git clone -q https://chromium.googlesource.com/chromium/tools/depot_tools.git $DEPOT_TOOLS

  if [ $UNAME = 'Windows' ]; then
    # set up task to run gclient.bat to get python
    schtasks //Create //tn init_gclient //tr `cd $DEPOT_TOOLS; pwd -W`/gclient.bat //sc onstart //f //RU system
    schtasks //Run //tn init_gclient
    sleep 1
    schtasks //Delete //tn init_gclient //f
    while [ ! -f $DEPOT_TOOLS/python276_bin/python.exe ]; do
      sleep 5
    done
  fi
fi
