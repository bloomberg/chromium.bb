#!/bin/bash

NPM_ROOT=$(npm root)
CLOSURE_DIR="${NPM_ROOT}/google-closure-compiler"
PATH_TO_COMPILER="${CLOSURE_DIR}/compiler.jar"
PATH_TO_EXTERNS_CHROME="${CLOSURE_DIR}/contrib/externs/chrome_extensions.js"

java -jar $PATH_TO_COMPILER \
  --checks-only --language_in=ECMASCRIPT6 --warning_level=VERBOSE \
  --externs=externs_js/polymer.js --externs=$PATH_TO_EXTERNS_CHROME \
  --externs=externs_js/chrome.js \
  js/unpacker.js js/*.js
