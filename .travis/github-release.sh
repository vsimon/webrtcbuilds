#!/bin/bash
set -eo pipefail
set -x

REPO=$1 && shift
RELEASE=$1 && shift
RELEASEFILES=$@
TAG=$RELEASE

if [[ -z "$RELEASEFILES" ]]; then
  echo "Error: No release files provided"
  exit 1
fi

set +x
if [[ -z "$GITHUBTOKEN" ]]; then
  echo "Error: GITHUBTOKEN is not set"
  exit 1
fi
set -x

echo -n "Checking release exists for $RELEASE..."

set +x
RESULT=`curl -s -w "\n%{http_code}\n"     \
  -H "Authorization: token $GITHUBTOKEN"  \
  "https://api.github.com/repos/$REPO/releases/tags/$TAG"`
set -x

RELEASEID=`echo "$RESULT" | jq -s '.[0]? | .id'`

if [[ "`echo "$RESULT" | tail -1`" == "404" || $RELEASEID == 'null' ]]; then
  echo NO
  echo "Creating GitHub release for $RELEASE"

  echo -n "Create release... "
JSON=$(cat <<EOF
{
  "tag_name":         "$TAG",
  "target_commitish": "master",
  "name":             "WebRTC Revision $TAG",
  "draft":            false,
  "prerelease":       false
}
EOF
)
  set +x
  RESULT=`curl -s -w "\n%{http_code}\n"     \
    -H "Authorization: token $GITHUBTOKEN"  \
    -d "$JSON"                              \
    "https://api.github.com/repos/$REPO/releases"`
  set -x
  if [ "`echo "$RESULT" | tail -1`" != "201" ]; then
    echo FAILED
    echo "$RESULT"
    exit 1
  fi
  echo DONE
else
  echo YES
fi

RELEASEID=`echo "$RESULT" | jq -s '.[0]? | .id'`
if [[ -z "$RELEASEID" ]]; then
  echo FAILED
  echo "$RESULT"
  exit 1
fi

for FILE in $RELEASEFILES; do
  if [ ! -f $FILE ]; then
    echo "Warning: $FILE not a file"
    continue
  fi
  FILENAME=`basename $FILE`

  URL=`echo "$RESULT" | jq -r -s ".[0]? | .assets[] | select(.browser_download_url | endswith(\"$FILENAME\")) | .url"`
  if [ ! -z $URL ]; then
    echo -n "Deleting $FILENAME..."
    set +x
    RESULT=`curl -s -w "%{http_code}\n"                    \
       -H "Authorization: token $GITHUBTOKEN"              \
       -X DELETE $URL`
    set -x
    if [[ "`echo "$RESULT"`" != "204" ]]; then
      echo FAILED
      echo "$RESULT"
      exit 1
    fi
    echo DONE
  fi

  echo -n "Uploading $FILENAME... "
  set +x
  RESULT=`curl -s -w "\n%{http_code}\n"                   \
    -H "Authorization: token $GITHUBTOKEN"                \
    -H "Accept: application/vnd.github.manifold-preview"  \
    -H "Content-Type: application/zip"                    \
    --data-binary "@$FILE"                                \
    "https://uploads.github.com/repos/$REPO/releases/$RELEASEID/assets?name=$FILENAME"`
  set -x
  if [ "`echo "$RESULT" | tail -1`" != "201" ]; then
    echo FAILED
    echo "$RESULT"
    exit 1
  fi
  echo DONE
done
