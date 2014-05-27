# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
    'includes': [
        'v8/v8.gypi',
    ],
    'variables': {
        'bindings_dir': '.',
        'blink_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink',
        'bindings_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink/bindings',
        'bindings_unittest_files': [
            '<@(bindings_v8_unittest_files)',
        ],
        'conditions': [
            # The bindings generator can skip writing generated files if they
            # are identical to the already existing file, which avoids
            # recompilation.  However, a dependency (earlier build step) having
            # a newer timestamp than an output (later build step) confuses some
            # build systems, so only use this on ninja, which explicitly
            # supports this use case (gyp turns all actions into ninja restat
            # rules).
            ['"<(GENERATOR)"=="ninja"', {
              'write_file_only_if_changed': '1',
            }, {
              'write_file_only_if_changed': '0',
            }],
        ],
    },
}
