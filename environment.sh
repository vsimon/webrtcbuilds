#!/bin/bash

# These are some common environment variables

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

export PROJECT_NAME=webrtcbuilds
export REPO_URL="https://chromium.googlesource.com/external/webrtc"

export UNAME=`uname`
if [ "$UNAME" != "Darwin" -a "$UNAME" != "Linux" ]; then
  if [ "$OS" = 'Windows_NT' ]; then
    export UNAME='Windows_NT'
  else
    echo "Building on unsupported platform"
    exit 1
  fi
fi

if [ "$UNAME" = "Linux" ]; then
  export PLATFORM=linux64
  export OUT_DIR=$DIR/out
  export JAVA_HOME=/usr/lib/jvm/java-7-openjdk-amd64
elif [ "$UNAME" = "Windows_NT" ]; then
  export PLATFORM=windows
  export OUT_DIR=$DIR/out
elif [ "$UNAME" = "Darwin" ]; then
  export PLATFORM=osx
  export OUT_DIR=$DIR/out
fi

mkdir -p $OUT_DIR

export DEPOT_TOOLS=$DIR/depot_tools
export PATH=$DEPOT_TOOLS:$PATH
if [ "$UNAME" = "Windows_NT" ]; then
  export DEPOT_TOOLS_WIN_TOOLCHAIN=0
  export WIN_DEPOT_TOOLS=`cd $DEPOT_TOOLS; pwd -W`
fi

# for extensibility
if [ -f $DIR/environment.local ]; then
  source $DIR/environment.local
fi
