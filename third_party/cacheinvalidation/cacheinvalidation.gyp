# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(akalin): Make it so that warnings are errors on Windows.
# TODO(akalin): Clean up warnings on Windows.

{
  'variables': {
    # The root directory for the proto files.
    'proto_dir_root': 'files/src',
    # The relative path of the cacheinvalidation proto files from
    # proto_dir_root.
    # TODO(akalin): Add a RULE_INPUT_DIR predefined variable to gyp so
    # we don't need this variable.
    'proto_dir_relpath': 'google/cacheinvalidation',
    # Where files generated from proto files are put.
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
  },
  'targets': [
    # The rule/action to generate files from the cacheinvalidation proto
    # files.
    {
      'target_name': 'cacheinvalidation_proto',
      'type': 'none',
      'sources': [
        '<(proto_dir_root)/google/cacheinvalidation/internal.proto',
        '<(proto_dir_root)/google/cacheinvalidation/ticl_persistence.proto',
        '<(proto_dir_root)/google/cacheinvalidation/types.proto',
      ],
      # TODO(akalin): This block was copied from the sync_proto target
      # from chrome.gyp.  Decomp the shared blocks out somehow.
      'rules': [
        {
          'rule_name': 'genproto',
          'extension': 'proto',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(protoc_out_dir)/<(proto_dir_relpath)/<(RULE_INPUT_ROOT).pb.h',
            '<(protoc_out_dir)/<(proto_dir_relpath)/<(RULE_INPUT_ROOT).pb.cc',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=<(proto_dir_root)',
            # This path needs to be prefixed by proto_path, so we can't
            # use RULE_INPUT_PATH (which is an absolute path).
            '<(proto_dir_root)/<(proto_dir_relpath)/<(RULE_INPUT_NAME)',
            '--cpp_out=<(protoc_out_dir)',
          ],
          'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
        },
      ],
      'dependencies': [
        '../../third_party/protobuf/protobuf.gyp:protoc#host',
      ],
      # This target exports a hard dependency because it generates header
      # files.
      'hard_dependency': 1,
    },
    # The main cache invalidation library.  External clients should depend
    # only on this.
    {
      'target_name': 'cacheinvalidation',
      'type': '<(library)',
      'sources': [
        '<(protoc_out_dir)/<(proto_dir_relpath)/internal.pb.h',
        '<(protoc_out_dir)/<(proto_dir_relpath)/internal.pb.cc',
        '<(protoc_out_dir)/<(proto_dir_relpath)/ticl_persistence.pb.h',
        '<(protoc_out_dir)/<(proto_dir_relpath)/ticl_persistence.pb.cc',
        '<(protoc_out_dir)/<(proto_dir_relpath)/types.pb.h',
        '<(protoc_out_dir)/<(proto_dir_relpath)/types.pb.cc',
        'overrides/google/cacheinvalidation/callback.h',
        'overrides/google/cacheinvalidation/compiler-specific.h',
        'overrides/google/cacheinvalidation/gmock.h',
        'overrides/google/cacheinvalidation/googletest.h',
        'overrides/google/cacheinvalidation/hash_map.h',
        'overrides/google/cacheinvalidation/logging.h',
        'overrides/google/cacheinvalidation/md5.h',
        'overrides/google/cacheinvalidation/mutex.h',
        'overrides/google/cacheinvalidation/random.h',
        'overrides/google/cacheinvalidation/scoped_ptr.h',
        'overrides/google/cacheinvalidation/stl-namespace.h'
        'overrides/google/cacheinvalidation/string_util.h'
        'overrides/google/cacheinvalidation/time.h',
        'files/src/google/cacheinvalidation/invalidation-client-impl.cc',
        'files/src/google/cacheinvalidation/invalidation-client-impl.h',
        'files/src/google/cacheinvalidation/invalidation-client.cc',
        'files/src/google/cacheinvalidation/invalidation-client.h',
        'files/src/google/cacheinvalidation/invalidation-types.h',
        'files/src/google/cacheinvalidation/log-macro.h',
        'files/src/google/cacheinvalidation/network-manager.cc',
        'files/src/google/cacheinvalidation/network-manager.h',
        'files/src/google/cacheinvalidation/persistence-manager.cc',
        'files/src/google/cacheinvalidation/persistence-manager.h',
        'files/src/google/cacheinvalidation/persistence-utils.cc',
        'files/src/google/cacheinvalidation/persistence-utils.h',
        'files/src/google/cacheinvalidation/proto-converter.cc',
        'files/src/google/cacheinvalidation/proto-converter.h',
        'files/src/google/cacheinvalidation/registration-update-manager.cc',
        'files/src/google/cacheinvalidation/registration-update-manager.h',
        'files/src/google/cacheinvalidation/session-manager.cc',
        'files/src/google/cacheinvalidation/session-manager.h',
        'files/src/google/cacheinvalidation/throttle.cc',
        'files/src/google/cacheinvalidation/throttle.h',
        'files/src/google/cacheinvalidation/version-manager.cc',
        'files/src/google/cacheinvalidation/version-manager.h',
      ],
      'include_dirs': [
        '<(protoc_out_dir)',
        './overrides',
        './files/src',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'cacheinvalidation_proto',
      ],
      # This target exports a hard dependency because depedents require
      # cacheinvalidation_proto to compile.
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          '<(protoc_out_dir)',
          './overrides',
          './files/src',
        ],
      },
      'export_dependent_settings': [
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'cacheinvalidation_proto',
      ],
    },
    # Unittests for the cache invalidation library.
    {
      'target_name': 'cacheinvalidation_unittests',
      'type': 'executable',
      'sources': [
        '../../base/test/run_all_unittests.cc',
        'files/src/google/cacheinvalidation/system-resources-for-test.h',
        'files/src/google/cacheinvalidation/invalidation-client-impl_test.cc',
        'files/src/google/cacheinvalidation/persistence-manager_test.cc',
        'files/src/google/cacheinvalidation/persistence-utils_test.cc',
        'files/src/google/cacheinvalidation/throttle_test.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        'cacheinvalidation',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
