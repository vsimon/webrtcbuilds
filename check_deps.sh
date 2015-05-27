#!/bin/bash
set -eo pipefail
set -x

# This installs all dependencies for building.

# osx deps: homebrew
# lin deps: apt-get

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/environment.sh

if [ "$UNAME" = "Darwin" ]; then
  # for GNU version of cp: gcp and jq
  brew install \
    coreutils \
    jq
elif [ "$UNAME" = "Linux" ]; then
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
    libjpeg62-dev \
    libxv-dev \
    libgtk2.0-dev \
    libexpat1-dev \
    libxtst-dev \
    libxss-dev \
    libudev-dev \
    libgconf2-dev \
    libgnome-keyring-dev \
    libpci-dev \
    libgl1-mesa-dev
fi

# for extensibility
if [ -f $DIR/check_deps.local ]; then
  $DIR/check_deps.local
fi
