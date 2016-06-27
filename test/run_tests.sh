#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
WEBRTCBUILDS_FOLDER="$1"
WEBRTCBUILDS_FOLDER=$(readlink -f $WEBRTCBUILDS_FOLDER)
export PKG_CONFIG_PATH=$WEBRTCBUILDS_FOLDER/lib/Release/pkgconfig

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT INT TERM HUP
pushd $tmpdir
  c++ -o simple_app $DIR/simple_app.cc -D_GLIBCXX_USE_CXX11_ABI=0 $(pkg-config --cflags --libs --define-variable=WEBRTC_LOCAL=$WEBRTCBUILDS_FOLDER libwebrtc_full) -lpthread
  ./simple_app
popd
