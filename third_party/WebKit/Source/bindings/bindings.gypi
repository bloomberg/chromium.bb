# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
    'includes': [
        'v8/v8.gypi',
    ],
    'variables': {
        'blink_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink',
        'bindings_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink/bindings',
        'bindings_unittest_files': [
            '<@(bindings_v8_unittest_files)',
        ],
        'conditions': [
            ['OS=="win" and buildtype=="Official"', {
                # On Windows Official release builds, we try to preserve symbol
                # space.
                'bindings_core_generated_aggregate_files': [
                    '<(bindings_output_dir)/V8GeneratedCoreBindings.cpp',
                ],
                'bindings_modules_generated_aggregate_files': [
                    '<(bindings_output_dir)/V8GeneratedModulesBindings.cpp',
                ],
            }, {
                'bindings_core_generated_aggregate_files': [
                    '<(bindings_output_dir)/V8GeneratedCoreBindings01.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings02.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings03.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings04.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings05.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings06.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings07.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings08.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings09.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings10.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings11.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings12.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings13.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings14.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings15.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings16.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings17.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings18.cpp',
                    '<(bindings_output_dir)/V8GeneratedCoreBindings19.cpp',
                ],
                'bindings_modules_generated_aggregate_files': [
                    '<(bindings_output_dir)/V8GeneratedModulesBindings01.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings02.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings03.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings04.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings05.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings06.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings07.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings08.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings09.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings10.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings11.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings12.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings13.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings14.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings15.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings16.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings17.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings18.cpp',
                    '<(bindings_output_dir)/V8GeneratedModulesBindings19.cpp',
                ],
            }],

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
