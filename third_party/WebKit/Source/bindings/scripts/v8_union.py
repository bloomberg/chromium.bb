# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def union_context(union_types):
    return {
        'containers': [container_context(union_type)
                       for union_type in union_types],
    }


def container_context(union_type):
    return {
        'cpp_class': union_type.name,
    }
