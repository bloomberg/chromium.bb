# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../bindings.gypi',  # FIXME: factor out bindings_core http://crbug.com/358074
    '../modules/v8/generated.gypi',  # FIXME: remove once bindings CG generates qualified includes http://crbug.com/377364
    'v8/generated.gypi',
  ],

  'variables': {
    'bindings_core_output_dir': '<(bindings_output_dir)/core',
  },
}
