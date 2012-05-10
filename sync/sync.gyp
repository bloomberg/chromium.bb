# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    # The core sync library.
    #
    # TODO(akalin): Rename this to something like 'sync_core' and
    # reserve the 'sync' name for the overarching library that clients
    # should depend on.
    {
      'target_name': 'sync',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../crypto/crypto.gyp:crypto',
        '../net/net.gyp:net',
        '../sql/sql.gyp:sql',
        'protocol/sync_proto.gyp:sync_proto',
      ],
      'export_dependent_settings': [
        # Propagate sync_proto since our headers include its generated
        # files.
        'protocol/sync_proto.gyp:sync_proto',
      ],
      'sources': [
        'engine/apply_updates_command.cc',
        'engine/apply_updates_command.h',
        'engine/build_commit_command.cc',
        'engine/build_commit_command.h',
        'engine/cleanup_disabled_types_command.cc',
        'engine/cleanup_disabled_types_command.h',
        'engine/clear_data_command.cc',
        'engine/clear_data_command.h',
        'engine/conflict_resolver.cc',
        'engine/conflict_resolver.h',
        'engine/download_updates_command.cc',
        'engine/download_updates_command.h',
        'engine/get_commit_ids_command.cc',
        'engine/get_commit_ids_command.h',
        'engine/model_changing_syncer_command.cc',
        'engine/model_changing_syncer_command.h',
        'engine/model_safe_worker.cc',
        'engine/model_safe_worker.h',
        'engine/passive_model_worker.cc',
        'engine/passive_model_worker.h',
        'engine/net/server_connection_manager.cc',
        'engine/net/server_connection_manager.h',
        'engine/net/url_translator.cc',
        'engine/net/url_translator.h',
        'engine/nigori_util.cc',
        'engine/nigori_util.h',
        'engine/nudge_source.cc',
        'engine/nudge_source.h',
        'engine/polling_constants.cc',
        'engine/polling_constants.h',
        'engine/post_commit_message_command.cc',
        'engine/post_commit_message_command.h',
        'engine/process_commit_response_command.cc',
        'engine/process_commit_response_command.h',
        'engine/process_updates_command.cc',
        'engine/process_updates_command.h',
        'engine/resolve_conflicts_command.cc',
        'engine/resolve_conflicts_command.h',
        'engine/store_timestamps_command.cc',
        'engine/store_timestamps_command.h',
        'engine/syncer.cc',
        'engine/syncer.h',
        'engine/syncer_command.cc',
        'engine/syncer_command.h',
        'engine/sync_engine_event.cc',
        'engine/sync_engine_event.h',
        'engine/syncer_proto_util.cc',
        'engine/syncer_proto_util.h',
        'engine/sync_scheduler.cc',
        'engine/sync_scheduler.h',
        'engine/syncer_types.h',
        'engine/syncer_util.cc',
        'engine/syncer_util.h',
        'engine/syncproto.h',
        'engine/traffic_logger.cc',
        'engine/traffic_logger.h',
        'engine/traffic_recorder.cc',
        'engine/traffic_recorder.h',
        'engine/update_applicator.cc',
        'engine/update_applicator.h',
        'engine/verify_updates_command.cc',
        'engine/verify_updates_command.h',
        'js/js_arg_list.cc',
        'js/js_arg_list.h',
        'js/js_backend.h',
        'js/js_controller.h',
        'js/js_event_details.cc',
        'js/js_event_details.h',
        'js/js_event_handler.h',
        'js/js_reply_handler.h',
        'js/sync_js_controller.cc',
        'js/sync_js_controller.h',
        'protocol/proto_enum_conversions.cc',
        'protocol/proto_enum_conversions.h',
        'protocol/proto_value_conversions.cc',
        'protocol/proto_value_conversions.h',
        'protocol/service_constants.h',
        'protocol/sync_protocol_error.cc',
        'protocol/sync_protocol_error.h',
        'sessions/debug_info_getter.h',
        'sessions/ordered_commit_set.cc',
        'sessions/ordered_commit_set.h',
        'sessions/session_state.cc',
        'sessions/session_state.h',
        'sessions/status_controller.cc',
        'sessions/status_controller.h',
        'sessions/sync_session.cc',
        'sessions/sync_session.h',
        'sessions/sync_session_context.cc',
        'sessions/sync_session_context.h',
        'syncable/blob.h',
        'syncable/directory_backing_store.cc',
        'syncable/directory_backing_store.h',
        'syncable/directory_change_delegate.h',
        'syncable/dir_open_result.h',
        'syncable/in_memory_directory_backing_store.cc',
        'syncable/in_memory_directory_backing_store.h',
        'syncable/model_type.cc',
        'syncable/model_type.h',
        'syncable/model_type_payload_map.cc',
        'syncable/model_type_payload_map.h',
        'syncable/on_disk_directory_backing_store.cc',
        'syncable/on_disk_directory_backing_store.h',
        'syncable/syncable.cc',
        'syncable/syncable_changes_version.h',
        'syncable/syncable_columns.h',
        'syncable/syncable_enum_conversions.cc',
        'syncable/syncable_enum_conversions.h',
        'syncable/syncable.h',
        'syncable/syncable_id.cc',
        'syncable/syncable_id.h',
        'syncable/syncable-inl.h',
        'syncable/transaction_observer.h',
        'util/cryptographer.cc',
        'util/cryptographer.h',

        # TODO(akalin): Figure out a better place to put
        # data_encryption_win*; it's also used by autofill.
        'util/data_encryption_win.cc',
        'util/data_encryption_win.h',

        'util/data_type_histogram.h',
        'util/encryptor.h',
        'util/enum_set.h',
        'util/experiments.h',
        'util/extensions_activity_monitor.cc',
        'util/extensions_activity_monitor.h',
        'util/get_session_name.cc',
        'util/get_session_name.h',
        'util/get_session_name_mac.mm',
        'util/get_session_name_mac.h',
        'util/get_session_name_win.cc',
        'util/get_session_name_win.h',
        'util/immutable.h',
        'util/logging.cc',
        'util/logging.h',
        'util/nigori.cc',
        'util/nigori.h',
        'util/report_unrecoverable_error_function.h',
        'util/session_utils_android.cc',
        'util/session_utils_android.h',
        'util/syncer_error.cc',
        'util/syncer_error.h',
        'util/time.cc',
        'util/time.h',
        'util/unrecoverable_error_handler.h',
        'util/unrecoverable_error_info.h',
        'util/unrecoverable_error_info.cc',
        'util/weak_handle.cc',
        'util/weak_handle.h',
      ],
    },

    # The sync notifications library.
    {
      'target_name': 'sync_notifier',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../jingle/jingle.gyp:notifier',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        'sync',
      ],
      'export_dependent_settings': [
        '../jingle/jingle.gyp:notifier',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
      ],
      'sources': [
        'notifier/sync_notifier.h',
        'notifier/sync_notifier_factory.h',
        'notifier/sync_notifier_factory.cc',
        'notifier/sync_notifier_observer.h',
      ],
      'conditions': [
        ['OS != "android"', {
          'sources': [
            'notifier/cache_invalidation_packet_handler.cc',
            'notifier/cache_invalidation_packet_handler.h',
            'notifier/chrome_invalidation_client.cc',
            'notifier/chrome_invalidation_client.h',
            'notifier/chrome_system_resources.cc',
            'notifier/chrome_system_resources.h',
            'notifier/invalidation_notifier.h',
            'notifier/invalidation_notifier.cc',
            'notifier/invalidation_util.cc',
            'notifier/invalidation_util.h',
            'notifier/invalidation_version_tracker.h',
            'notifier/non_blocking_invalidation_notifier.h',
            'notifier/non_blocking_invalidation_notifier.cc',
            'notifier/p2p_notifier.h',
            'notifier/p2p_notifier.cc',
            'notifier/registration_manager.cc',
            'notifier/registration_manager.h',
            'notifier/state_writer.h',
          ],
        }],
      ],
    },

    # The sync internal API library.
    {
      'target_name': 'syncapi_core',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../net/net.gyp:net',
        'protocol/sync_proto.gyp:sync_proto',
        'sync_notifier',
        'sync',
      ],
      'export_dependent_settings': [
        # Propagate sync_proto since our headers include its generated
        # files.
        'protocol/sync_proto.gyp:sync_proto',
        'sync_notifier',
        'sync',
      ],
      'sources': [
        'internal_api/all_status.cc',
        'internal_api/all_status.h',
        'internal_api/base_node.cc',
        'internal_api/base_node.h',
        'internal_api/base_transaction.cc',
        'internal_api/base_transaction.h',
        'internal_api/change_record.cc',
        'internal_api/change_record.h',
        'internal_api/change_reorder_buffer.cc',
        'internal_api/change_reorder_buffer.h',
        'internal_api/configure_reason.h',
        'internal_api/debug_info_event_listener.cc',
        'internal_api/debug_info_event_listener.h',
        'internal_api/http_post_provider_factory.h',
        'internal_api/http_post_provider_interface.h',
        'internal_api/js_mutation_event_observer.cc',
        'internal_api/js_mutation_event_observer.h',
        'internal_api/js_sync_manager_observer.cc',
        'internal_api/js_sync_manager_observer.h',
        'internal_api/read_node.cc',
        'internal_api/read_node.h',
        'internal_api/read_transaction.cc',
        'internal_api/read_transaction.h',
        'internal_api/syncapi_internal.cc',
        'internal_api/syncapi_internal.h',
        'internal_api/syncapi_server_connection_manager.cc',
        'internal_api/syncapi_server_connection_manager.h',
        'internal_api/sync_manager.cc',
        'internal_api/sync_manager.h',
        'internal_api/user_share.cc',
        'internal_api/user_share.h',
        'internal_api/write_node.cc',
        'internal_api/write_node.h',
        'internal_api/write_transaction.cc',
        'internal_api/write_transaction.h',
      ],
    },

    # Test support files for the 'sync' target.
    {
      'target_name': 'test_support_sync',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'protocol/sync_proto.gyp:sync_proto',
        'sync',
      ],
      'export_dependent_settings': [
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'protocol/sync_proto.gyp:sync_proto',
        'sync',
      ],
      'sources': [
        'js/js_test_util.cc',
        'js/js_test_util.h',
        'sessions/test_util.cc',
        'sessions/test_util.h',
        'syncable/model_type_test_util.cc',
        'syncable/model_type_test_util.h',
        'syncable/syncable_mock.cc',
        'syncable/syncable_mock.h',
        'test/fake_encryptor.cc',
        'test/fake_encryptor.h',
        'test/fake_extensions_activity_monitor.cc',
        'test/fake_extensions_activity_monitor.h',
        'test/null_directory_change_delegate.cc',
        'test/null_directory_change_delegate.h',
        'test/null_transaction_observer.cc',
        'test/null_transaction_observer.h',
        'test/engine/test_directory_setter_upper.cc',
        'test/engine/test_directory_setter_upper.h',
        'test/engine/fake_model_safe_worker_registrar.cc',
        'test/engine/fake_model_safe_worker_registrar.h',
        'test/engine/fake_model_worker.cc',
        'test/engine/fake_model_worker.h',
        'test/engine/mock_connection_manager.cc',
        'test/engine/mock_connection_manager.h',
        'test/engine/syncer_command_test.cc',
        'test/engine/syncer_command_test.h',
        'test/engine/test_id_factory.h',
        'test/engine/test_syncable_utils.cc',
        'test/engine/test_syncable_utils.h',
        'test/sessions/test_scoped_session_event_listener.h',
        'test/test_directory_backing_store.cc',
        'test/test_directory_backing_store.h',
        'util/test_unrecoverable_error_handler.cc',
        'util/test_unrecoverable_error_handler.h',
      ],
    },

    # Test support files for the 'sync_notifier' target.
    {
      'target_name': 'test_support_sync_notifier',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../testing/gmock.gyp:gmock',
        'sync_notifier',
      ],
      'export_dependent_settings': [
        '../testing/gmock.gyp:gmock',
        'sync_notifier',
      ],
      'sources': [
        'notifier/mock_sync_notifier_observer.cc',
        'notifier/mock_sync_notifier_observer.h',
      ],
    },

    # Test support files for the 'syncapi_core' target.
    {
      'target_name': 'test_support_syncapi_core',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'syncapi_core',
        'test_support_sync',
      ],
      'export_dependent_settings': [
        '../testing/gtest.gyp:gtest',
        'syncapi_core',
        'test_support_sync',
      ],
      'sources': [
        'internal_api/test_user_share.cc',
        'internal_api/test_user_share.h',
      ],
    },

    # Unit tests for the 'sync' target.  This cannot be a static
    # library because the unit test files have to be compiled directly
    # into the executable, so we push the target files to the
    # depending executable target via direct_dependent_settings.
    {
      'target_name': 'sync_tests',
      'type': 'none',
      # We only want unit test executables to include this target.
      'suppress_wildcard': 1,
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'protocol/sync_proto.gyp:sync_proto',
        'sync',
        'test_support_sync',
      ],
      # Propagate all dependencies since the actual compilation
      # happens in the dependents.
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'protocol/sync_proto.gyp:sync_proto',
        'sync',
        'test_support_sync',
      ],
      'direct_dependent_settings': {
        'variables': { 'enable_wexit_time_destructors': 1, },
        'include_dirs': [
          '..',
        ],
        'sources': [
          'engine/apply_updates_command_unittest.cc',
          'engine/build_commit_command_unittest.cc',
          'engine/clear_data_command_unittest.cc',
          'engine/cleanup_disabled_types_command_unittest.cc',
          'engine/download_updates_command_unittest.cc',
          'engine/model_changing_syncer_command_unittest.cc',
          'engine/model_safe_worker_unittest.cc',
          'engine/nigori_util_unittest.cc',
          'engine/process_commit_response_command_unittest.cc',
          'engine/process_updates_command_unittest.cc',
          'engine/resolve_conflicts_command_unittest.cc',
          'engine/syncer_proto_util_unittest.cc',
          'engine/sync_scheduler_unittest.cc',
          'engine/sync_scheduler_whitebox_unittest.cc',
          'engine/syncer_unittest.cc',
          'engine/syncproto_unittest.cc',
          'engine/traffic_recorder_unittest.cc',
          'engine/verify_updates_command_unittest.cc',
          'js/js_arg_list_unittest.cc',
          'js/js_event_details_unittest.cc',
          'js/sync_js_controller_unittest.cc',
          'protocol/proto_enum_conversions_unittest.cc',
          'protocol/proto_value_conversions_unittest.cc',
          'sessions/ordered_commit_set_unittest.cc',
          'sessions/session_state_unittest.cc',
          'sessions/status_controller_unittest.cc',
          'sessions/sync_session_context_unittest.cc',
          'sessions/sync_session_unittest.cc',
          'syncable/directory_backing_store_unittest.cc',
          'syncable/model_type_payload_map_unittest.cc',
          'syncable/model_type_unittest.cc',
          'syncable/syncable_enum_conversions_unittest.cc',
          'syncable/syncable_id_unittest.cc',
          'syncable/syncable_unittest.cc',
          'util/cryptographer_unittest.cc',
          'util/data_encryption_win_unittest.cc',
          'util/data_type_histogram_unittest.cc',
          'util/enum_set_unittest.cc',
          'util/get_session_name_unittest.cc',
          'util/immutable_unittest.cc',
          'util/nigori_unittest.cc',
          'util/protobuf_unittest.cc',
          'util/weak_handle_unittest.cc',
        ],
      },
    },

    # Unit tests for the 'sync_notifier' target.  This cannot be a static
    # library because the unit test files have to be compiled directly
    # into the executable, so we push the target files to the
    # depending executable target via direct_dependent_settings.
    {
      'target_name': 'sync_notifier_tests',
      'type': 'none',
      # We only want unit test executables to include this target.
      'suppress_wildcard': 1,
      'dependencies': [
        '../base/base.gyp:base',
        '../jingle/jingle.gyp:notifier_test_util',
        '../net/net.gyp:net_test_support',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        'sync',
        'sync_notifier',
        'test_support_sync_notifier',
      ],
      # Propagate all dependencies since the actual compilation
      # happens in the dependents.
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../jingle/jingle.gyp:notifier_test_util',
        '../net/net.gyp:net_test_support',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        'sync',
        'sync_notifier',
        'test_support_sync_notifier',
      ],
      'direct_dependent_settings': {
        'variables': { 'enable_wexit_time_destructors': 1, },
        'include_dirs': [
          '..',
        ],
        'sources': [
          'notifier/sync_notifier_factory_unittest.cc',
        ],
        'conditions': [
          ['OS != "android"', {
            'sources': [
              'notifier/cache_invalidation_packet_handler_unittest.cc',
              'notifier/chrome_invalidation_client_unittest.cc',
              'notifier/chrome_system_resources_unittest.cc',
              'notifier/invalidation_notifier_unittest.cc',
              'notifier/non_blocking_invalidation_notifier_unittest.cc',
              'notifier/p2p_notifier_unittest.cc',
              'notifier/registration_manager_unittest.cc',
            ],
          }],
        ],
      },
    },

    # Unit tests for the 'syncapi_core' target.  This cannot be a static
    # library because the unit test files have to be compiled directly
    # into the executable, so we push the target files to the
    # depending executable target via direct_dependent_settings.
    {
      'target_name': 'syncapi_core_tests',
      'type': 'none',
      # We only want unit test executables to include this target.
      'suppress_wildcard': 1,
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'protocol/sync_proto.gyp:sync_proto',
        'sync',
        'sync_notifier',
        'syncapi_core',
        'test_support_syncapi_core',
      ],
      # Propagate all dependencies since the actual compilation
      # happens in the dependents.
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'protocol/sync_proto.gyp:sync_proto',
        'sync',
        'sync_notifier',
        'syncapi_core',
        'test_support_syncapi_core',
      ],
      'direct_dependent_settings': {
        'variables': { 'enable_wexit_time_destructors': 1, },
        'include_dirs': [
          '..',
        ],
        'sources': [
          'internal_api/change_record_unittest.cc',
          'internal_api/debug_info_event_listener_unittest.cc',
          'internal_api/js_mutation_event_observer_unittest.cc',
          'internal_api/js_sync_manager_observer_unittest.cc',
          'internal_api/syncapi_server_connection_manager_unittest.cc',
          'internal_api/syncapi_unittest.cc',
        ],
      },
    },

    # The unit test executable for sync tests.
    {
      'target_name': 'sync_unit_tests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:run_all_unittests',
        'sync_tests',
        'sync_notifier_tests',
        'syncapi_core_tests',
      ],
      # TODO(akalin): This is needed because histogram.cc uses
      # leak_annotations.h, which pulls this in.  Make 'base'
      # propagate this dependency.
      'conditions': [
        ['OS=="linux" and linux_use_tcmalloc==1', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['OS=="linux" and chromeos==1', {
            # TODO(kochi): Remove this once we get rid of dependency from
            # get_session_name.cc.
            'dependencies': [
                '../chrome/chrome.gyp:browser',
            ],
        }],
      ],
    },

    # A tool to listen to sync notifications and print them out.
    {
      'target_name': 'sync_listen_notifications',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../jingle/jingle.gyp:notifier',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        'sync',
        'sync_notifier',
      ],
      'sources': [
        'tools/sync_listen_notifications.cc',
      ],
    },
  ],
}
