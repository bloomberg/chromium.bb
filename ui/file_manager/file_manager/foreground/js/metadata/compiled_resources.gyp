# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'metadata_dispatcher',
      'variables': {
        'depends': [
          'metadata_parser.js',
          'byte_reader.js',
          'exif_parser.js',
          'image_parsers.js',
          'mpeg_parser.js',
          'id3_parser.js',
	],
	'externs': [
          '../../../../externs/platform_worker.js',
	]
      },
      'includes': [
        '../../../../../../third_party/closure_compiler/compile_js.gypi'
      ],
    }
  ]
}
