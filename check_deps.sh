#!/bin/bash
set -eo pipefail
set -x

# This installs all dependencies for building.

# osx deps: homebrew
# lin deps: apt-get
# win deps: curl

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/environment.sh

if [ $UNAME = 'Darwin' ]; then
  # for GNU version of cp: gcp and jq
  which jq || brew install jq
  which gsed || brew install gnu-sed
  which gcp || brew install coreutils
elif [ $UNAME = 'Linux' ]; then
  sudo apt-get update && \
  sudo apt-get install -y --no-install-recommends \
    curl \
    wget \
    git \
    jq \
    python \
    python-pip \
    openjdk-7-jdk \
    g++ \
    zip \
    ruby \
    libnss3-dev \
    libasound2-dev \
    libpulse-dev \
    libjpeg-dev \
    libxv-dev \
    libgtk2.0-dev \
    libexpat1-dev \
    libxtst-dev \
    libxss-dev \
    libudev-dev \
    libgconf2-dev \
    libgnome-keyring-dev \
    libpci-dev \
    libgl1-mesa-dev \
    lib32stdc++6 \
    lib32z1
else
  # put jq in python_bin in depot_tools because it is ignored by git
  curl -L -o $DEPOT_TOOLS/python276_bin/jq.exe https://github.com/stedolan/jq/releases/download/jq-1.4/jq-win32.exe
fi

# for extensibility
if [ -f $DIR/check_deps.local ]; then
  $DIR/check_deps.local
fi
