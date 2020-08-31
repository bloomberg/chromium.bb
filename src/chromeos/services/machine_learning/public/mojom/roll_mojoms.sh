#!/bin/bash

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

usage() {
  echo "Usage: $0 CROS_SRC_DIR"
  echo ""
  echo "Rolls (copies) ML Service *.mojom files from Chrome OS to current"
  echo "directory, with appropriate boilerplate modifications for use in"
  echo "Chromium."
  echo ""
  echo "CROS_SRC_DIR: Path to Chromium OS source, e.g. ~/chromiumos/src."
}

CROS_SRC_DIR="$1"

if [ -z "$CROS_SRC_DIR" ]; then
  usage
  exit 1
fi

if [ ! -d "$CROS_SRC_DIR" ]; then
  echo "$CROS_SRC_DIR not a directory"
  usage
  exit 1
fi

if [ "$(basename $CROS_SRC_DIR)" != "src" ]; then
  echo "$CROS_SRC_DIR should end with /src"
  usage
  exit 1
fi

if ! git diff-index --quiet HEAD -- ; then
  echo "'git status' not clean. Commit your changes first."
  exit 1
fi

readonly EXPECTED_PATH="chromeos/services/machine_learning/public/mojom"
if [[ "$(pwd)" != *"${EXPECTED_PATH}" ]]; then
  echo "Please run from within ${EXPECTED_PATH}."
  exit 1;
fi

echo "Copying mojoms from Chrome OS side ..."
cp $1/platform2/ml/mojom/*.mojom . || exit 1

echo "Removing time.mojom ..."
rm time.mojom || exit 1

echo "Changing import paths ..."
sed --in-place --regexp-extended \
  -e 's~^import "ml/mojom/time.mojom~import "mojo/public/mojom/base/time.mojom~g' \
  -e 's~^import "ml~import "chromeos/services/machine_learning/public~g' \
  *.mojom

echo "Applying Mojo syntax changes ..."
sed --in-place --regexp-extended \
  -e 's/(GraphExecutor|Model|TextClassifier)& request/pending_receiver<\1> receiver/g' \
  *.mojom

echo "OK. Now examine 'git diff' to double-check."
