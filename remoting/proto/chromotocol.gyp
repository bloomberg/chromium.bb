# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out/remoting/proto',
  },
  'targets': [
    {
      # Protobuf compiler / generate rule for chromoting.proto.
      'target_name': 'chromotocol_proto',
      'type': 'none',
      'sources': [
        'auth.proto',
        'control.proto',
        'event.proto',
        'internal.proto',
        'video.proto',
      ],
      'rules': [
        {
          'rule_name': 'genproto',
          'extension': 'proto',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(out_dir)/<(RULE_INPUT_ROOT).pb.cc',
            '<(out_dir)/<(RULE_INPUT_ROOT).pb.h',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=.',
            './<(RULE_INPUT_ROOT)<(RULE_INPUT_EXT)',
            '--cpp_out=<(out_dir)',
          ],
          'message': 'Generating C++ and Python code from <(RULE_INPUT_PATH)',
        },
      ],
      'dependencies': [
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protoc#host',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
        ],
      },
      # This target exports a hard dependency because it generates header
      # files.
      'hard_dependency': 1,
    },

    {
      'target_name': 'chromotocol_proto_lib',
      'type': 'static_library',
      'export_dependent_settings': [
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
        'chromotocol_proto',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
        'chromotocol_proto',
      ],
      # This target exports a hard dependency because depedents require
      # chromotocol_proto to compile.
      'hard_dependency': 1,
      'sources': [
        '<(out_dir)/auth.pb.cc',
        '<(out_dir)/auth.pb.h',
        '<(out_dir)/control.pb.cc',
        '<(out_dir)/control.pb.h',
        '<(out_dir)/event.pb.cc',
        '<(out_dir)/event.pb.h',
        '<(out_dir)/internal.pb.cc',
        '<(out_dir)/internal.pb.h',
        '<(out_dir)/video.pb.cc',
        '<(out_dir)/video.pb.h',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
