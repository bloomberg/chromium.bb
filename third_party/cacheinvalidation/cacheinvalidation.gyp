# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This library should build cleanly with the extra warnings turned on
    # for Chromium.
    'chromium_code': 1,
    # The root directory for the proto files.
    'proto_dir_root': 'files/src',
    # The relative path of the cacheinvalidation proto files from
    # proto_dir_root.
    # TODO(akalin): Add a RULE_INPUT_DIR predefined variable to gyp so
    # we don't need this variable.
    # TODO(ghc): Remove v2/ dir and move all files up a level.
    'proto_dir_relpath': 'google/cacheinvalidation/v2',
    # Where files generated from proto files are put.
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
    # The path to the protoc executable.
    'protoc': '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
  },
  'targets': [
    # The rule/action to generate files from the cacheinvalidation proto
    # files.
    {
      'target_name': 'cacheinvalidation_proto',
      'type': 'none',
      'sources': [
        '<(proto_dir_root)/<(proto_dir_relpath)/client.proto',
        '<(proto_dir_root)/<(proto_dir_relpath)/client_protocol.proto',
        '<(proto_dir_root)/<(proto_dir_relpath)/client_test_internal.proto',
        '<(proto_dir_root)/<(proto_dir_relpath)/types.proto',
      ],
      # TODO(akalin): This block was copied from the sync_proto target
      # from chrome.gyp.  Decomp the shared blocks out somehow.
      'rules': [
        {
          'rule_name': 'genproto',
          'extension': 'proto',
          'inputs': [
            '<(protoc)',
          ],
          'outputs': [
            '<(protoc_out_dir)/<(proto_dir_relpath)/<(RULE_INPUT_ROOT).pb.h',
            '<(protoc_out_dir)/<(proto_dir_relpath)/<(RULE_INPUT_ROOT).pb.cc',
          ],
          'action': [
            '<(protoc)',
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
    },
    # The C++ files generated from the cache invalidation protocol buffers.
    {
      'target_name': 'cacheinvalidation_proto_cpp',
      'type': 'static_library',
      'sources': [
        '<(protoc_out_dir)/<(proto_dir_relpath)/client.pb.h',
        '<(protoc_out_dir)/<(proto_dir_relpath)/client.pb.cc',
        '<(protoc_out_dir)/<(proto_dir_relpath)/client_protocol.pb.h',
        '<(protoc_out_dir)/<(proto_dir_relpath)/client_protocol.pb.cc',
        '<(protoc_out_dir)/<(proto_dir_relpath)/client_test_internal.pb.h',
        '<(protoc_out_dir)/<(proto_dir_relpath)/client_test_internal.pb.cc',
        '<(protoc_out_dir)/<(proto_dir_relpath)/types.pb.h',
        '<(protoc_out_dir)/<(proto_dir_relpath)/types.pb.cc',
      ],
      'dependencies': [
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'cacheinvalidation_proto',
      ],
      'include_dirs': [
        '<(protoc_out_dir)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(protoc_out_dir)',
        ],
      },
      'export_dependent_settings': [
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
      # This target exports a hard dependency because it contains generated
      # header files.
      'hard_dependency': 1,
    },
    # The main cache invalidation library.  External clients should depend
    # only on this.
    {
      'target_name': 'cacheinvalidation',
      'type': 'static_library',
      'sources': [
        'overrides/google/cacheinvalidation/callback.h',
        'overrides/google/cacheinvalidation/compiler-specific.h',
        'overrides/google/cacheinvalidation/gmock.h',
        'overrides/google/cacheinvalidation/googletest.h',
        'overrides/google/cacheinvalidation/v2/logging.h',
        'overrides/google/cacheinvalidation/md5.h',
        'overrides/google/cacheinvalidation/v2/mutex.h',
        'overrides/google/cacheinvalidation/random.h',
        'overrides/google/cacheinvalidation/v2/scoped_ptr.h',
        'overrides/google/cacheinvalidation/stl-namespace.h',
        'overrides/google/cacheinvalidation/v2/string_util.h',
        'overrides/google/cacheinvalidation/v2/time.h',
        'files/src/google/cacheinvalidation/v2/checking-invalidation-listener.cc',
        'files/src/google/cacheinvalidation/v2/checking-invalidation-listener.h',
        'files/src/google/cacheinvalidation/v2/client-protocol-namespace-fix.h',
        'files/src/google/cacheinvalidation/v2/constants.h',
        'files/src/google/cacheinvalidation/v2/constants.cc',
        'files/src/google/cacheinvalidation/v2/digest-function.h',
        'files/src/google/cacheinvalidation/v2/digest-store.h',
        'files/src/google/cacheinvalidation/v2/exponential-backoff-delay-generator.h',
        'files/src/google/cacheinvalidation/v2/exponential-backoff-delay-generator.cc',
        'files/src/google/cacheinvalidation/v2/invalidation-client-impl.cc',
        'files/src/google/cacheinvalidation/v2/invalidation-client-impl.h',
        'files/src/google/cacheinvalidation/v2/invalidation-client.h',
        'files/src/google/cacheinvalidation/v2/invalidation-client-util.h',
        'files/src/google/cacheinvalidation/v2/invalidation-listener.h',
        'files/src/google/cacheinvalidation/v2/log-macro.h',
        'files/src/google/cacheinvalidation/v2/logging.h',
        'files/src/google/cacheinvalidation/v2/mutex.h',
        'files/src/google/cacheinvalidation/v2/object-id-digest-utils.cc',
        'files/src/google/cacheinvalidation/v2/object-id-digest-utils.h',
        'files/src/google/cacheinvalidation/v2/operation-scheduler.cc',
        'files/src/google/cacheinvalidation/v2/operation-scheduler.h',
        'files/src/google/cacheinvalidation/v2/persistence-utils.cc',
        'files/src/google/cacheinvalidation/v2/persistence-utils.h',
        'files/src/google/cacheinvalidation/v2/proto-converter.cc',
        'files/src/google/cacheinvalidation/v2/proto-converter.h',
        'files/src/google/cacheinvalidation/v2/proto-helpers.h',
        'files/src/google/cacheinvalidation/v2/protocol-handler.cc',
        'files/src/google/cacheinvalidation/v2/protocol-handler.h',
        'files/src/google/cacheinvalidation/v2/registration-manager.cc',
        'files/src/google/cacheinvalidation/v2/registration-manager.h',
        'files/src/google/cacheinvalidation/v2/run-state.h',
        'files/src/google/cacheinvalidation/v2/sha1-digest-function.h',
        'files/src/google/cacheinvalidation/v2/simple-registration-store.cc',
        'files/src/google/cacheinvalidation/v2/simple-registration-store.h',
        'files/src/google/cacheinvalidation/v2/smearer.h',
        'files/src/google/cacheinvalidation/v2/statistics.cc',
        'files/src/google/cacheinvalidation/v2/statistics.h',
        'files/src/google/cacheinvalidation/v2/string_util.h',
        'files/src/google/cacheinvalidation/v2/system-resources.h',
        'files/src/google/cacheinvalidation/v2/ticl-message-validator.h',
        'files/src/google/cacheinvalidation/v2/time.h',
        'files/src/google/cacheinvalidation/v2/types.h',
      ],
      'include_dirs': [
        './overrides',
        './files/src',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        'cacheinvalidation_proto',
        'cacheinvalidation_proto_cpp',
      ],
      # This target exports a hard dependency because its include files
      # include generated header files from cache_invalidation_proto_cpp.
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          './overrides',
          './files/src',
        ],
      },
      'export_dependent_settings': [
        '../../base/base.gyp:base',
        'cacheinvalidation_proto_cpp',
      ],
    },
    # Unittests for the cache invalidation library.
    # TODO(ghc): Write native tests and include them here.
    {
      'target_name': 'cacheinvalidation_unittests',
      'type': 'executable',
      'sources': [
        '../../base/test/run_all_unittests.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        # Needed by run_all_unittests.cc.
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
