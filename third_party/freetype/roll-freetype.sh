#!/bin/bash

REVIEWERS=`paste -s -d, third_party/freetype/OWNERS` &&
roll-dep -r "${REVIEWERS}" "$@" src/third_party/freetype/src/ &&

TMPFILE=`mktemp` &&
git -C third_party/freetype/src/ cat-file blob HEAD@{1}:include/freetype/config/ftoption.h >> ${TMPFILE} &&
git merge-file third_party/freetype/include/freetype-custom-config/ftoption.h ${TMPFILE} third_party/freetype/src/include/freetype/config/ftoption.h &&
rm ${TMPFILE} &&
git add third_party/freetype/include/freetype-custom-config/ftoption.h &&

TMPFILE=`mktemp` &&
git -C third_party/freetype/src/ cat-file blob HEAD@{1}:include/freetype/config/ftconfig.h >> ${TMPFILE} &&
git merge-file third_party/freetype/include/freetype-custom-config/ftconfig.h ${TMPFILE} third_party/freetype/src/include/freetype/config/ftconfig.h &&
rm ${TMPFILE} &&
git add third_party/freetype/include/freetype-custom-config/ftconfig.h &&

FTVERSION=`git -C third_party/freetype/src/ describe` &&
FTCOMMIT=`git -C third_party/freetype/src/ rev-parse HEAD` &&
sed -i "s/^Version: .*\$/Version: ${FTVERSION%-*}/" third_party/freetype/README.chromium &&
sed -i "s/^Revision: .*\$/Revision: ${FTCOMMIT}/" third_party/freetype/README.chromium &&
git add third_party/freetype/README.chromium &&

git commit --quiet --amend --no-edit &&

true
