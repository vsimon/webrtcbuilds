#!/bin/bash
set -eo pipefail
set -x

REPO=$1 && shift
RELEASE=$1 && shift
MIRRORREPO=$1
TAG=$RELEASE

if [[ -z "$MIRRORREPO" ]]; then
  echo "Error: No repo to mirror provided"
  exit 1
fi

[ -e "$DIR/GITHUBTOKEN" ] && . "$DIR/GITHUBTOKEN"
if [[ -z "$GITHUBTOKEN" ]]; then
  echo "Error: GITHUBTOKEN is not set"
  exit 1
fi

pushd $MIRRORREPO

git remote add github "https://$GITHUBTOKEN@github.com/vsimon/webrtcbuilds.git"
git fetch github

echo -n "Checking tag already exists exists for $RELEASE..."

if ! git show-ref --tags | egrep -q $RELEASE ; then
  echo NO
  git push github master
  git tag $RELEASE
  git push github --tags
fi
echo YES

popd
