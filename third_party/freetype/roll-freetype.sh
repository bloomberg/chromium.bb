#!/bin/bash

rolldeps() {
  STEP="roll-deps" &&
  REVIEWERS=`paste -s -d, third_party/freetype/OWNERS` &&
  roll-dep -r "${REVIEWERS}" "$@" src/third_party/freetype/src/
}

checkmodules() {
  STEP="check modules.cfg: check list of modules and dependencies" &&
  ! git -C third_party/freetype/src/ diff --name-only HEAD@{1} | grep -q modules.cfg
}

mergeinclude() {
  INCLUDE=$1 &&
  STEP="merge ${INCLUDE}: check for merge conflicts" &&
  TMPFILE=`mktemp` &&
  git -C third_party/freetype/src/ cat-file blob HEAD@{1}:include/freetype/config/${INCLUDE} >> ${TMPFILE} &&
  git merge-file third_party/freetype/include/freetype-custom-config/${INCLUDE} ${TMPFILE} third_party/freetype/src/include/freetype/config/${INCLUDE} &&
  rm ${TMPFILE} &&
  git add third_party/freetype/include/freetype-custom-config/${INCLUDE}
}

updatereadme() {
  STEP="update README.chromium" &&
  FTVERSION=`git -C third_party/freetype/src/ describe --long` &&
  FTCOMMIT=`git -C third_party/freetype/src/ rev-parse HEAD` &&
  sed -i "s/^Version: .*\$/Version: ${FTVERSION%-*}/" third_party/freetype/README.chromium &&
  sed -i "s/^Revision: .*\$/Revision: ${FTCOMMIT}/" third_party/freetype/README.chromium &&
  git add third_party/freetype/README.chromium
}

commit() {
  STEP="commit" &&
  git commit --quiet --amend --no-edit
}

rolldeps "$@" &&
checkmodules &&
mergeinclude ftoption.h &&
mergeinclude ftconfig.h &&
updatereadme &&
commit &&
true || echo "Failed step ${STEP}" && false
