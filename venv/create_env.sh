#!/bin/bash
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cd "$(dirname "$(readlink -f -- "$0")")"
virtualenv venv --extra-search-dir=pip_packages
source venv/bin/activate
pip install --no-index -f pip_packages -r requirements.txt
