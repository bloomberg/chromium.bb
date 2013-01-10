# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'json_schema_compiler_tests',
      'type': 'static_library',
      'variables': {
        'chromium_code': 1,
        'json_schema_files': [
          'any.json',
          'additional_properties.json',
          'arrays.json',
          'callbacks.json',
          'choices.json',
          'crossref.json',
          'enums.json',
          'functions_as_parameters.json',
          'functions_on_types.json',
          'objects.json',
          'simple_api.json',
        ],
        'idl_schema_files': [
          'idl_basics.idl',
          'idl_object_types.idl'
        ],
        'cc_dir': 'tools/json_schema_compiler/test',
        'root_namespace': 'test::api',
      },
      'inputs': [
        '<@(idl_schema_files)',
      ],
      'sources': [
        '<@(json_schema_files)',
        '<@(idl_schema_files)',
      ],
      'includes': ['../../../build/json_schema_compile.gypi'],
    },
  ],
}
