#!/bin/bash
#
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Note how we don't actually link anything here.

# sed always does greedy matching. This is ok in the compiler wrapper scripts
# because
TARGETOBJ=`echo $@ | perl -pe 's/.*-o ([^\s]+).*/$1/;'`

touch $TARGETOBJ
