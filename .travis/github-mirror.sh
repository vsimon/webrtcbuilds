#!/bin/bash
set -eo pipefail

REPO=$1 && shift
RELEASE=$1 && shift
MIRRORREPO=$1
TAG=$RELEASE

if [[ -z "$MIRRORREPO" ]]; then
  echo "Error: No repo to mirror provided"
  exit 1
fi

set +x
if [[ -z "$GITHUBTOKEN" ]]; then
  echo "Error: GITHUBTOKEN is not set"
  exit 1
fi
set -x

pushd $MIRRORREPO

set +x
git remote add github "https://$GITHUBTOKEN@github.com/vsimon/webrtcbuilds.git"
set -x
git fetch github

echo -n "Checking tag already exists exists for $RELEASE..."

if ! git show-ref --tags | egrep -q refs/tags/$RELEASE ; then
  echo NO
  git reset --hard
  git tag $RELEASE
  git checkout master
  git pull origin master
  git push -q github master
  git push -q github --tags
fi
echo YES

popd
