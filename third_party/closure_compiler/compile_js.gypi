# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'type': 'none',
  'variables': {
    'CLOSURE_DIR': '<(DEPTH)/third_party/closure_compiler',
    'EXTERNS_DIR': '<(CLOSURE_DIR)/externs',
  },
  'actions': [
    {
      # This action optionally takes these arguments:
      # - depends: scripts that the source file depends on being included already
      # - externs: files that describe globals used by |source|
      'action_name': 'compile_js',
      'variables': {
        'source_file': '<(_target_name).js',
        'out_file': '<(SHARED_INTERMEDIATE_DIR)/closure/<!(python <(CLOSURE_DIR)/build/outputs.py <@(source_file))',
        'externs%': [],
        'depends%': [],
      },
      'inputs': [
        'compile_js.gypi',
        '<(CLOSURE_DIR)/compile.py',
        '<(CLOSURE_DIR)/processor.py',
        '<(CLOSURE_DIR)/build/inputs.py',
        '<(CLOSURE_DIR)/build/outputs.py',
        '<(CLOSURE_DIR)/compiler/compiler.jar',
        '<(CLOSURE_DIR)/runner/runner.jar',
        '<!@(python <(CLOSURE_DIR)/build/inputs.py <(source_file) -d <@(depends) -e <@(externs))',
      ],
      'outputs': [
        '<(out_file)',
      ],
      'action': [
        'python',
        '<(CLOSURE_DIR)/compile.py',
        '<(source_file)',
        '--depends', '<@(depends)',
        '--externs', '<@(externs)',
        '--out_file', '<(out_file)',
        # Add '--verbose' for make glorious log spam of Closure compiler.
      ],
      'message': 'Compiling <(source_file)',
    }
  ],
}
