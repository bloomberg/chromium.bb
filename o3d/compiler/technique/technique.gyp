# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 0,
    'technique_out_dir': '<(SHARED_INTERMEDIATE_DIR)/technique',
  },
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'o3dTechnique',
      'type': 'static_library',
      'dependencies': [
        '../../../<(antlrdir)/antlr.gyp:antlr3c',
        '../../../base/base.gyp:base',
        '../../core/core.gyp:o3dCore',
      ],
      'include_dirs': [
        '<(technique_out_dir)',
      ],
      'rules': [
        {
          'rule_name': 'technique_parser',
          'extension': 'g3pl',
          'inputs' : [
            '../../../<(antlrdir)/lib/antlr-3.1.1.jar',
          ],
          'outputs': [
            '<(technique_out_dir)/<(RULE_INPUT_ROOT)Lexer.c',
            '<(technique_out_dir)/<(RULE_INPUT_ROOT)Lexer.h',
            '<(technique_out_dir)/<(RULE_INPUT_ROOT)Parser.c',
            '<(technique_out_dir)/<(RULE_INPUT_ROOT)Parser.h',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'java',
            '-cp', '../../../<(antlrdir)/lib/antlr-3.1.1.jar',
            'org.antlr.Tool',
            '<(RULE_INPUT_PATH)',
            '-fo', '<(technique_out_dir)',
          ],
        },
      ],
      'sources': [
        'Technique.g3pl',
        'technique_error.cc',
        'technique_error.h',
        'technique_parser.cc',
        'technique_parser.h',
        'technique_structures.cc',
        'technique_structures.h',
      ],
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          '<(technique_out_dir)',
        ],
      },
      'msvs_settings': {
        'VCCLCompilerTool': {
          'CompileAs': '2',
        },
      },
      'xcode_settings': {
        'OTHER_CFLAGS': ['-x', 'c++'],
      },
    },
    {
      'target_name': 'o3dTechniqueTest',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/unittest_data',
          'files': [
            'test_data/fur.fx',
            'test_data/lambert.fx',
            'test_data/noshader.fx',
            'test_data/notechnique.fx',
            'test_data/sampler_test.fx',
            'test_data/shadow_map.fx',
            'test_data/simple.fx',
          ],
        },
      ],
    },
  ],
}
