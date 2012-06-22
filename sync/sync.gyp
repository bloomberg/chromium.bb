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
        'internal_api/public/engine/model_safe_worker.cc',
        'internal_api/public/engine/model_safe_worker.h',
        'internal_api/public/engine/passive_model_worker.cc',
        'internal_api/public/engine/passive_model_worker.h',
        'internal_api/public/engine/polling_constants.cc',
        'internal_api/public/engine/polling_constants.h',
        'internal_api/public/engine/sync_status.cc',
        'internal_api/public/engine/sync_status.h',
        'internal_api/public/sessions/model_neutral_state.cc',
        'internal_api/public/sessions/model_neutral_state.h',
        'internal_api/public/sessions/sync_session_snapshot.cc',
        'internal_api/public/sessions/sync_session_snapshot.h',
        'internal_api/public/sessions/sync_source_info.cc',
        'internal_api/public/sessions/sync_source_info.h',
        'internal_api/public/syncable/model_type.h',
        'internal_api/public/syncable/model_type_payload_map.cc',
        'internal_api/public/syncable/model_type_payload_map.h',
        'internal_api/public/util/enum_set.h',
        'internal_api/public/util/experiments.h',
        'internal_api/public/util/immutable.h',
        'internal_api/public/util/syncer_error.cc',
        'internal_api/public/util/syncer_error.h',
        'internal_api/public/util/report_unrecoverable_error_function.h',
        'internal_api/public/util/unrecoverable_error_handler.h',
        'internal_api/public/util/unrecoverable_error_info.h',
        'internal_api/public/util/unrecoverable_error_info.cc',
        'internal_api/public/util/weak_handle.cc',
        'internal_api/public/util/weak_handle.h',
        'engine/all_status.cc',
        'engine/all_status.h',
        'engine/apply_updates_command.cc',
        'engine/apply_updates_command.h',
        'engine/build_commit_command.cc',
        'engine/build_commit_command.h',
        'engine/cleanup_disabled_types_command.cc',
        'engine/cleanup_disabled_types_command.h',
        'engine/commit.cc',
        'engine/commit.h',
        'engine/conflict_resolver.cc',
        'engine/conflict_resolver.h',
        'engine/download_updates_command.cc',
        'engine/download_updates_command.h',
        'engine/get_commit_ids_command.cc',
        'engine/get_commit_ids_command.h',
        'engine/model_changing_syncer_command.cc',
        'engine/model_changing_syncer_command.h',
        'engine/net/server_connection_manager.cc',
        'engine/net/server_connection_manager.h',
        'engine/net/url_translator.cc',
        'engine/net/url_translator.h',
        'engine/nigori_util.cc',
        'engine/nigori_util.h',
        'engine/nudge_source.cc',
        'engine/nudge_source.h',
        'engine/process_commit_response_command.cc',
        'engine/process_commit_response_command.h',
        'engine/process_updates_command.cc',
        'engine/process_updates_command.h',
        'engine/resolve_conflicts_command.cc',
        'engine/resolve_conflicts_command.h',
        'engine/store_timestamps_command.cc',
        'engine/store_timestamps_command.h',
        'engine/sync_engine_event.cc',
        'engine/sync_engine_event.h',
        'engine/sync_scheduler.cc',
        'engine/sync_scheduler.h',
        'engine/syncer.cc',
        'engine/syncer.h',
        'engine/syncer_command.cc',
        'engine/syncer_command.h',
        'engine/syncer_proto_util.cc',
        'engine/syncer_proto_util.h',
        'engine/syncer_types.h',
        'engine/syncer_util.cc',
        'engine/syncer_util.h',
        'engine/syncproto.h',
        'engine/throttled_data_type_tracker.cc',
        'engine/throttled_data_type_tracker.h',
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
        'syncable/base_transaction.cc',
        'syncable/base_transaction.h',
        'syncable/blob.h',
        'syncable/dir_open_result.h',
        'syncable/directory.cc',
        'syncable/directory.h',
        'syncable/directory_backing_store.cc',
        'syncable/directory_backing_store.h',
        'syncable/directory_change_delegate.h',
        'syncable/entry.cc',
        'syncable/entry.h',
        'syncable/entry_kernel.cc',
        'syncable/entry_kernel.h',
        'syncable/in_memory_directory_backing_store.cc',
        'syncable/in_memory_directory_backing_store.h',
        'syncable/metahandle_set.h',
        'syncable/model_type.cc',
        'syncable/mutable_entry.cc',
        'syncable/mutable_entry.h',
        'syncable/on_disk_directory_backing_store.cc',
        'syncable/on_disk_directory_backing_store.h',
        'syncable/read_transaction.cc',
        'syncable/read_transaction.h',
        'syncable/scoped_kernel_lock.h',
        'syncable/syncable-inl.h',
        'syncable/syncable_changes_version.h',
        'syncable/syncable_columns.h',
        'syncable/syncable_enum_conversions.cc',
        'syncable/syncable_enum_conversions.h',
        'syncable/syncable_id.cc',
        'syncable/syncable_id.h',
        'syncable/syncable_util.cc',
        'syncable/syncable_util.h',
        'syncable/transaction_observer.h',
        'syncable/write_transaction.cc',
        'syncable/write_transaction.h',
        'syncable/write_transaction_info.cc',
        'syncable/write_transaction_info.h',
        'util/cryptographer.cc',
        'util/cryptographer.h',

        # TODO(akalin): Figure out a better place to put
        # data_encryption_win*; it's also used by autofill.
        'util/data_encryption_win.cc',
        'util/data_encryption_win.h',

        'util/data_type_histogram.h',
        'util/encryptor.h',
        'util/extensions_activity_monitor.cc',
        'util/extensions_activity_monitor.h',
        'util/get_session_name.cc',
        'util/get_session_name.h',
        'util/get_session_name_mac.mm',
        'util/get_session_name_mac.h',
        'util/get_session_name_win.cc',
        'util/get_session_name_win.h',
        'util/logging.cc',
        'util/logging.h',
        'util/nigori.cc',
        'util/nigori.h',
        'util/session_utils_android.cc',
        'util/session_utils_android.h',
        'util/time.cc',
        'util/time.h',
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
        # TODO(akalin): Remove this (http://crbug.com/133352).
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_proto_cpp',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        'sync',
      ],
      'export_dependent_settings': [
        '../jingle/jingle.gyp:notifier',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
      ],
      'sources': [
        'notifier/invalidation_util.cc',
        'notifier/invalidation_util.h',
        'notifier/notifications_disabled_reason.h',
        'notifier/notifications_disabled_reason.cc',
        'notifier/sync_notifier.h',
        'notifier/sync_notifier_factory.h',
        'notifier/sync_notifier_factory.cc',
        'notifier/sync_notifier_observer.h',
      ],
      'conditions': [
        ['OS != "android"', {
          'sources': [
            'notifier/chrome_invalidation_client.cc',
            'notifier/chrome_invalidation_client.h',
            'notifier/chrome_system_resources.cc',
            'notifier/chrome_system_resources.h',
            'notifier/invalidation_notifier.h',
            'notifier/invalidation_notifier.cc',
            'notifier/invalidation_state_tracker.h',
            'notifier/non_blocking_invalidation_notifier.h',
            'notifier/non_blocking_invalidation_notifier.cc',
            'notifier/p2p_notifier.h',
            'notifier/p2p_notifier.cc',
            'notifier/push_client_channel.cc',
            'notifier/push_client_channel.h',
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
        'sync',
      ],
      'sources': [
        'internal_api/public/base_node.h',
        'internal_api/public/base_transaction.h',
        'internal_api/public/change_record.h',
        'internal_api/public/configure_reason.h',
        'internal_api/public/http_bridge.h',
        'internal_api/public/http_post_provider_factory.h',
        'internal_api/public/http_post_provider_interface.h',
        'internal_api/public/read_node.h',
        'internal_api/public/read_transaction.h',
        'internal_api/public/sync_manager.h',
        'internal_api/public/user_share.h',
        'internal_api/public/write_node.h',
        'internal_api/public/write_transaction.h',
        'internal_api/base_node.cc',
        'internal_api/base_transaction.cc',
        'internal_api/change_record.cc',
        'internal_api/change_reorder_buffer.cc',
        'internal_api/change_reorder_buffer.h',
        'internal_api/debug_info_event_listener.cc',
        'internal_api/debug_info_event_listener.h',
        'internal_api/http_bridge.cc',
        'internal_api/js_mutation_event_observer.cc',
        'internal_api/js_mutation_event_observer.h',
        'internal_api/js_sync_manager_observer.cc',
        'internal_api/js_sync_manager_observer.h',
        'internal_api/read_node.cc',
        'internal_api/read_transaction.cc',
        'internal_api/syncapi_internal.cc',
        'internal_api/syncapi_internal.h',
        'internal_api/syncapi_server_connection_manager.cc',
        'internal_api/syncapi_server_connection_manager.h',
        'internal_api/sync_manager.cc',
        'internal_api/user_share.cc',
        'internal_api/write_node.cc',
        'internal_api/write_transaction.cc',
      ],
    },

    # The sync external API library.
    {
      'target_name': 'syncapi_service',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'protocol/sync_proto.gyp:sync_proto',
        'sync',
      ],
      # We avoid including header files from sync_proto in our public
      # header files so we don't need to export its settings.
      'sources': [
        'api/syncable_service.cc',
        'api/syncable_service.h',
        'api/sync_data.h',
        'api/sync_data.cc',
        'api/sync_change.h',
        'api/sync_change.cc',
        'api/sync_change_processor.h',
        'api/sync_change_processor.cc',
        'api/sync_error.h',
        'api/sync_error.cc',
        'api/sync_error_factory.h',
        'api/sync_error_factory.cc',
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
        'internal_api/public/syncable/model_type_test_util.cc',
        'internal_api/public/syncable/model_type_test_util.h',
        'js/js_test_util.cc',
        'js/js_test_util.h',
        'sessions/test_util.cc',
        'sessions/test_util.h',
        'syncable/syncable_mock.cc',
        'syncable/syncable_mock.h',
        'test/engine/fake_model_worker.cc',
        'test/engine/fake_model_worker.h',
        'test/engine/mock_connection_manager.cc',
        'test/engine/mock_connection_manager.h',
        'test/engine/syncer_command_test.cc',
        'test/engine/syncer_command_test.h',
        'test/engine/test_directory_setter_upper.cc',
        'test/engine/test_directory_setter_upper.h',
        'test/engine/test_id_factory.h',
        'test/engine/test_syncable_utils.cc',
        'test/engine/test_syncable_utils.h',
        'test/fake_encryptor.cc',
        'test/fake_encryptor.h',
        'test/fake_extensions_activity_monitor.cc',
        'test/fake_extensions_activity_monitor.h',
        'test/null_directory_change_delegate.cc',
        'test/null_directory_change_delegate.h',
        'test/null_transaction_observer.cc',
        'test/null_transaction_observer.h',
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
        'notifier/mock_invalidation_state_tracker.cc',
        'notifier/mock_invalidation_state_tracker.h',
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
        'internal_api/public/test/test_user_share.h',
        'internal_api/test/test_user_share.cc',
      ],
    },

    # Test support files for the 'syncapi_service' target.
    {
      'target_name': 'test_support_syncapi_service',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../testing/gmock.gyp:gmock',
        'syncapi_service',
      ],
      'export_dependent_settings': [
        '../testing/gmock.gyp:gmock',
        'syncapi_service',
      ],
      'sources': [
        'api/fake_syncable_service.cc',
        'api/fake_syncable_service.h',
        'api/sync_error_factory_mock.cc',
        'api/sync_error_factory_mock.h',
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
          'internal_api/public/engine/model_safe_worker_unittest.cc',
          'internal_api/public/syncable/model_type_payload_map_unittest.cc',
          'internal_api/public/util/enum_set_unittest.cc',
          'internal_api/public/util/immutable_unittest.cc',
          'engine/apply_updates_command_unittest.cc',
          'engine/build_commit_command_unittest.cc',
          'engine/cleanup_disabled_types_command_unittest.cc',
          'engine/download_updates_command_unittest.cc',
          'engine/model_changing_syncer_command_unittest.cc',
          'engine/nigori_util_unittest.cc',
          'engine/process_commit_response_command_unittest.cc',
          'engine/process_updates_command_unittest.cc',
          'engine/resolve_conflicts_command_unittest.cc',
          'engine/syncer_proto_util_unittest.cc',
          'engine/syncer_unittest.cc',
          'engine/syncproto_unittest.cc',
          'engine/sync_scheduler_unittest.cc',
          'engine/sync_scheduler_whitebox_unittest.cc',
          'engine/throttled_data_type_tracker_unittest.cc',
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
          'sessions/sync_session_unittest.cc',
          'syncable/directory_backing_store_unittest.cc',
          'syncable/model_type_unittest.cc',
          'syncable/syncable_enum_conversions_unittest.cc',
          'syncable/syncable_id_unittest.cc',
          'syncable/syncable_unittest.cc',
          'util/cryptographer_unittest.cc',
          'util/data_encryption_win_unittest.cc',
          'util/data_type_histogram_unittest.cc',
          'util/get_session_name_unittest.cc',
          'util/nigori_unittest.cc',
          'util/protobuf_unittest.cc',
          'internal_api/public/util/weak_handle_unittest.cc',
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
              'notifier/chrome_invalidation_client_unittest.cc',
              'notifier/chrome_system_resources_unittest.cc',
              'notifier/invalidation_notifier_unittest.cc',
              'notifier/non_blocking_invalidation_notifier_unittest.cc',
              'notifier/p2p_notifier_unittest.cc',
              'notifier/push_client_channel_unittest.cc',
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
        '../net/net.gyp:net_test_support',
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
        '../net/net.gyp:net_test_support',
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
          'internal_api/public/change_record_unittest.cc',
          'internal_api/debug_info_event_listener_unittest.cc',
          'internal_api/http_bridge_unittest.cc',
          'internal_api/js_mutation_event_observer_unittest.cc',
          'internal_api/js_sync_manager_observer_unittest.cc',
          'internal_api/syncapi_server_connection_manager_unittest.cc',
          'internal_api/syncapi_unittest.cc',
        ],
      },
    },

    # Unit tests for the 'syncapi_service' target.  This cannot be a static
    # library because the unit test files have to be compiled directly
    # into the executable, so we push the target files to the
    # depending executable target via direct_dependent_settings.
    {
      'target_name': 'syncapi_service_tests',
      'type': 'none',
      # We only want unit test executables to include this target.
      'suppress_wildcard': 1,
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'protocol/sync_proto.gyp:sync_proto',
        'sync',
        'syncapi_service',
        'test_support_syncapi_service',
      ],
      # Propagate all dependencies since the actual compilation
      # happens in the dependents.
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'protocol/sync_proto.gyp:sync_proto',
        'sync',
        'syncapi_service',
        'test_support_syncapi_service',
      ],
      'direct_dependent_settings': {
        'variables': { 'enable_wexit_time_destructors': 1, },
        'include_dirs': [
          '..',
        ],
        'sources': [
          'api/sync_change_unittest.cc',
          'api/sync_error_unittest.cc',
        ],
      },
    },

    # The unit test executable for sync tests.
    {
      'target_name': 'sync_unit_tests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:run_all_unittests',
        'sync_tests',
        'sync_notifier_tests',
        'syncapi_core_tests',
        'syncapi_service_tests',
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
        ['OS == "android" and gtest_target_type == "shared_library"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
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
  'conditions': [
    # Special target to wrap a gtest_target_type==shared_library
    # sync_unit_tests into an android apk for execution.
    ['OS == "android" and gtest_target_type == "shared_library"', {
      'targets': [
        {
          'target_name': 'sync_unit_tests_apk',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
            'sync_unit_tests',
          ],
          'variables': {
            'test_suite_name': 'sync_unit_tests',
            'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)sync_unit_tests<(SHARED_LIB_SUFFIX)',
            'input_jars_paths': [ '<(PRODUCT_DIR)/lib.java/chromium_base.jar', ],
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
    }],
  ],
}
