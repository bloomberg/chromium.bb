# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  },
  'targets': [
    # The rule/action to generate files from the cacheinvalidation proto
    # files and package them into a static library.
    {
      'target_name': 'cacheinvalidation_proto',
      'type': 'static_library',
      'sources': [
        '<(proto_dir_root)/<(proto_dir_relpath)/client.proto',
        '<(proto_dir_root)/<(proto_dir_relpath)/client_gateway.proto',
        '<(proto_dir_root)/<(proto_dir_relpath)/client_protocol.proto',
        '<(proto_dir_root)/<(proto_dir_relpath)/client_test_internal.proto',
        '<(proto_dir_root)/<(proto_dir_relpath)/types.proto',
      ],
      'variables': {
        'proto_in_dir': '<(proto_dir_root)',
        'proto_out_dir': '',
        'proto_relpath': '<(proto_dir_relpath)/',
        # This is necessary because these protos import with
        # qualified paths, such as:
        #   #import "google/cacheinvalidation/v2/client_protocol.proto"
        # rather than the more common form of:
        #   #import "client_protocol.proto"
        # NOTE: The trailing slash is required, see build/protoc.gypi
      },
      'includes': [ '../../build/protoc.gypi' ],
    },
    # The main cache invalidation library.  External clients should depend
    # only on this.
    {
      'target_name': 'cacheinvalidation',
      'type': 'static_library',
      'sources': [
        'overrides/google/cacheinvalidation/v2/callback.h',
        'overrides/google/cacheinvalidation/v2/gmock.h',
        'overrides/google/cacheinvalidation/v2/googletest.h',
        'overrides/google/cacheinvalidation/v2/logging.h',
        'overrides/google/cacheinvalidation/v2/mutex.h',
        'overrides/google/cacheinvalidation/v2/random.h',
        'overrides/google/cacheinvalidation/v2/scoped_ptr.h',
        'overrides/google/cacheinvalidation/v2/stl-namespace.h',
        'overrides/google/cacheinvalidation/v2/string_util.h',
        'overrides/google/cacheinvalidation/v2/time.h',
        'files/src/google/cacheinvalidation/v2/basic-system-resources.cc',
        'files/src/google/cacheinvalidation/v2/basic-system-resources.h',
        'files/src/google/cacheinvalidation/v2/checking-invalidation-listener.cc',
        'files/src/google/cacheinvalidation/v2/checking-invalidation-listener.h',
        'files/src/google/cacheinvalidation/v2/client-protocol-namespace-fix.h',
        'files/src/google/cacheinvalidation/v2/constants.cc',
        'files/src/google/cacheinvalidation/v2/constants.h',
        'files/src/google/cacheinvalidation/v2/digest-function.h',
        'files/src/google/cacheinvalidation/v2/digest-store.h',
        'files/src/google/cacheinvalidation/v2/exponential-backoff-delay-generator.cc',
        'files/src/google/cacheinvalidation/v2/exponential-backoff-delay-generator.h',
        'files/src/google/cacheinvalidation/v2/invalidation-client-factory.cc',
        'files/src/google/cacheinvalidation/v2/invalidation-client-factory.h',
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
        'files/src/google/cacheinvalidation/v2/proto-helpers.cc',
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
        'files/src/google/cacheinvalidation/v2/throttle.cc',
        'files/src/google/cacheinvalidation/v2/throttle.h',
        'files/src/google/cacheinvalidation/v2/ticl-message-validator.cc',
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
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          './overrides',
          './files/src',
        ],
      },
      'export_dependent_settings': [
        '../../base/base.gyp:base',
        'cacheinvalidation_proto',
      ],
    },
    # Unittests for the cache invalidation library.
    # TODO(ghc): Write native tests and include them here.
    {
      'target_name': 'cacheinvalidation_unittests',
      'type': 'executable',
      'sources': [
        '../../base/test/run_all_unittests.cc',
        'files/src/google/cacheinvalidation/v2/test/deterministic-scheduler.cc',
        'files/src/google/cacheinvalidation/v2/test/deterministic-scheduler.h',
        'files/src/google/cacheinvalidation/v2/test/test-logger.cc',
        'files/src/google/cacheinvalidation/v2/test/test-logger.h',
        'files/src/google/cacheinvalidation/v2/test/test-utils.cc',
        'files/src/google/cacheinvalidation/v2/test/test-utils.h',
        'files/src/google/cacheinvalidation/v2/invalidation-client-impl_test.cc',
        'files/src/google/cacheinvalidation/v2/protocol-handler_test.cc',
        'files/src/google/cacheinvalidation/v2/throttle_test.cc',
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
