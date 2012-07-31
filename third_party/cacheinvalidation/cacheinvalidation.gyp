# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This library should build cleanly with the extra warnings turned on
    # for Chromium.
    'chromium_code': 1,
    # The relative path of the cacheinvalidation proto files from
    # 'files/src'.
    # TODO(akalin): Add a RULE_INPUT_DIR predefined variable to gyp so
    # we don't need this variable.
    'proto_dir_relpath': 'google/cacheinvalidation',
    # Where files generated from proto files are put.
    'proto_in_dir': 'files/src/<(proto_dir_relpath)',
    'proto_out_dir': '<(proto_dir_relpath)',
  },
  'targets': [
    # The C++ files generated from the cache invalidation protocol buffers.
    {
      'target_name': 'cacheinvalidation_proto_cpp',
      'type': 'static_library',
      'sources': [
        '<(proto_in_dir)/client.proto',
        '<(proto_in_dir)/client_gateway.proto',
        '<(proto_in_dir)/client_protocol.proto',
        '<(proto_in_dir)/client_test_internal.proto',
        '<(proto_in_dir)/types.proto',
      ],
      'includes': [ '../../build/protoc.gypi' ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(proto_out_dir)',
        ],
      },
    },
    # The main cache invalidation library.  External clients should depend
    # only on this.
    {
      'target_name': 'cacheinvalidation',
      'type': 'static_library',
      'sources': [
        'overrides/google/cacheinvalidation/deps/callback.h',
        'overrides/google/cacheinvalidation/deps/gmock.h',
        'overrides/google/cacheinvalidation/deps/googletest.h',
        'overrides/google/cacheinvalidation/deps/logging.h',
        'overrides/google/cacheinvalidation/deps/mutex.h',
        'overrides/google/cacheinvalidation/deps/random.h',
        'overrides/google/cacheinvalidation/deps/sha1-digest-function.h',
        'overrides/google/cacheinvalidation/deps/scoped_ptr.h',
        'overrides/google/cacheinvalidation/deps/stl-namespace.h',
        'overrides/google/cacheinvalidation/deps/string_util.h',
        'overrides/google/cacheinvalidation/deps/time.h',
        'files/src/google/cacheinvalidation/deps/digest-function.h',
        'files/src/google/cacheinvalidation/impl/basic-system-resources.cc',
        'files/src/google/cacheinvalidation/impl/basic-system-resources.h',
        'files/src/google/cacheinvalidation/impl/checking-invalidation-listener.cc',
        'files/src/google/cacheinvalidation/impl/checking-invalidation-listener.h',
        'files/src/google/cacheinvalidation/impl/client-protocol-namespace-fix.h',
        'files/src/google/cacheinvalidation/impl/constants.cc',
        'files/src/google/cacheinvalidation/impl/constants.h',
        'files/src/google/cacheinvalidation/impl/digest-store.h',
        'files/src/google/cacheinvalidation/impl/exponential-backoff-delay-generator.cc',
        'files/src/google/cacheinvalidation/impl/exponential-backoff-delay-generator.h',
        'files/src/google/cacheinvalidation/impl/invalidation-client-factory.cc',
        'files/src/google/cacheinvalidation/impl/invalidation-client-impl.cc',
        'files/src/google/cacheinvalidation/impl/invalidation-client-impl.h',
        'files/src/google/cacheinvalidation/impl/invalidation-client-util.h',
        'files/src/google/cacheinvalidation/impl/log-macro.h',
        'files/src/google/cacheinvalidation/impl/object-id-digest-utils.cc',
        'files/src/google/cacheinvalidation/impl/object-id-digest-utils.h',
        'files/src/google/cacheinvalidation/impl/persistence-utils.cc',
        'files/src/google/cacheinvalidation/impl/persistence-utils.h',
        'files/src/google/cacheinvalidation/impl/proto-converter.cc',
        'files/src/google/cacheinvalidation/impl/proto-converter.h',
        'files/src/google/cacheinvalidation/impl/proto-helpers.h',
        'files/src/google/cacheinvalidation/impl/proto-helpers.cc',
        'files/src/google/cacheinvalidation/impl/protocol-handler.cc',
        'files/src/google/cacheinvalidation/impl/protocol-handler.h',
        'files/src/google/cacheinvalidation/impl/recurring-task.cc',
        'files/src/google/cacheinvalidation/impl/recurring-task.h',
        'files/src/google/cacheinvalidation/impl/registration-manager.cc',
        'files/src/google/cacheinvalidation/impl/registration-manager.h',
        'files/src/google/cacheinvalidation/impl/run-state.h',
        'files/src/google/cacheinvalidation/impl/safe-storage.cc',
        'files/src/google/cacheinvalidation/impl/safe-storage.h',
        'files/src/google/cacheinvalidation/impl/simple-registration-store.cc',
        'files/src/google/cacheinvalidation/impl/simple-registration-store.h',
        'files/src/google/cacheinvalidation/impl/smearer.h',
        'files/src/google/cacheinvalidation/impl/statistics.cc',
        'files/src/google/cacheinvalidation/impl/statistics.h',
        'files/src/google/cacheinvalidation/impl/throttle.cc',
        'files/src/google/cacheinvalidation/impl/throttle.h',
        'files/src/google/cacheinvalidation/impl/ticl-message-validator.cc',
        'files/src/google/cacheinvalidation/impl/ticl-message-validator.h',
        'files/src/google/cacheinvalidation/include/invalidation-client.h',
        'files/src/google/cacheinvalidation/include/invalidation-client-factory.h',
        'files/src/google/cacheinvalidation/include/invalidation-listener.h',
        'files/src/google/cacheinvalidation/include/system-resources.h',
        'files/src/google/cacheinvalidation/include/types.h',
      ],
      'include_dirs': [
        './overrides',
        './files/src',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        'cacheinvalidation_proto_cpp',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          './overrides',
          './files/src',
        ],
      },
      # We avoid including header files from
      # cacheinvalidation_proto_cpp in our public header files so we
      # don't need to export its settings.
      'export_dependent_settings': [
        '../../base/base.gyp:base',
      ],
    },
    # Unittests for the cache invalidation library.
    # TODO(ghc): Write native tests and include them here.
    {
      'target_name': 'cacheinvalidation_unittests',
      'type': 'executable',
      'sources': [
        'files/src/google/cacheinvalidation/test/deterministic-scheduler.cc',
        'files/src/google/cacheinvalidation/test/deterministic-scheduler.h',
        'files/src/google/cacheinvalidation/test/test-logger.cc',
        'files/src/google/cacheinvalidation/test/test-logger.h',
        'files/src/google/cacheinvalidation/test/test-utils.cc',
        'files/src/google/cacheinvalidation/test/test-utils.h',
        'files/src/google/cacheinvalidation/impl/invalidation-client-impl_test.cc',
        'files/src/google/cacheinvalidation/impl/protocol-handler_test.cc',
        'files/src/google/cacheinvalidation/impl/recurring-task_test.cc',
        'files/src/google/cacheinvalidation/impl/throttle_test.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:run_all_unittests',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        'cacheinvalidation',
        'cacheinvalidation_proto_cpp',
      ],
    },
    {
      'target_name': 'cacheinvalidation_unittests_run',
      'type': 'none',
      'dependencies': [
        'cacheinvalidation_unittests',
      ],
      'includes': [
        'cacheinvalidation_unittests.isolate',
      ],
      'actions': [
        {
          'action_name': 'isolate',
          'inputs': [
            'cacheinvalidation_unittests.isolate',
            '<@(isolate_dependency_tracked)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/cacheinvalidation_unittests.results',
          ],
          'action': [
            'python',
            '../../tools/isolate/isolate.py',
            '<(test_isolation_mode)',
            '--outdir', '<(test_isolation_outdir)',
            '--variable', 'PRODUCT_DIR', '<(PRODUCT_DIR)',
            '--variable', 'OS', '<(OS)',
            '--result', '<@(_outputs)',
            '--isolate', 'cacheinvalidation_unittests.isolate',
          ],
        },
      ],
    },
  ],
}
