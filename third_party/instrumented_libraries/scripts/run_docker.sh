#!/bin/bash
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

BINARY=$1
shift

BINARY_NAME=$(basename $BINARY)
BINARY_PATH=$(realpath $(dirname $BINARY))

docker run --rm --shm-size=2g --privileged --cap-add=all -ti -e HOST_UID=$UID -v $BINARY_PATH:/chrome trusty-chromium /chrome/$BINARY_NAME "$@"
