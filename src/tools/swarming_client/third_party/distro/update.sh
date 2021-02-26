#!/bin/bash
# Copyright 2020 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

cd $(dirname $0)

if [ $# != 1 ]; then
    echo "usage: $ $0 <version>"
    echo "e.g. $ $0 1.5.0"
    exit 1
fi

version="${1}"

find . | fgrep '/' | fgrep -v './update.sh' | fgrep -v 'README.swarming' | \
    sort -r | xargs rm -r
curl -sL https://github.com/nir0s/distro/archive/v${version}.tar.gz | \
    tar xvz --strip-component 1 --extract distro-${version}/distro.py \
        distro-${version}/LICENSE distro-${version}/README.md
mv distro.py __init__.py
