#!/bin/bash
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Unpacks an archive containing prebuilt instrumented libraries into output dir.

archive_file=$1
target_dir=$2
stamp_file=$3

rm ${target_dir}/* -rf
tar -zxf ${archive_file} -C ${target_dir}

touch ${stamp_file}
