#!/bin/bash
#
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Strip out any parameters that will make clang do anything other than syntax
# checking.
TARGETOBJ=`echo $@ | sed -e 's/.*-o \(.*\.o\).*/\1/;'`

ADJUSTED=`echo "$@" | sed -e 's/ -c / /g;' |\
                      sed -e 's/ -O0 / /g;' |\
                      sed -e 's/ -g / /g;' |\
                      sed -e 's/Google Inc/GoogleInc/g;' |\
                      sed -e 's/ -Wunused-value / /g;' |\
                      sed -e 's/ -Wswitch-enum / /g;' |\
                      sed -e 's/ -pipe / /g;' |\
                      sed -e 's/ -o.*\.o / /g;'`

clang++ -fsyntax-only $ADJUSTED && touch "$TARGETOBJ"
