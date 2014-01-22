# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'include_dirs': [
    '..',
  ],
  'defines': [
    'SYNC_IMPLEMENTATION',
  ],
  'dependencies': [
    '../base/base.gyp:base',
    '../jingle/jingle.gyp:jingle_glue',
    '../jingle/jingle.gyp:notifier',
    '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
    # TODO(akalin): Remove this (http://crbug.com/133352).
    '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_proto_cpp',
    '../third_party/libjingle/libjingle.gyp:libjingle',
  ],
  'export_dependent_settings': [
    '../jingle/jingle.gyp:notifier',
    '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
  ],
  'sources': [
    'notifier/ack_handler.cc',
    'notifier/ack_handler.h',
    'notifier/dropped_invalidation_tracker.cc',
    'notifier/dropped_invalidation_tracker.h',
    'notifier/invalidation_handler.h',
    'notifier/invalidation_state_tracker.h',
    'notifier/invalidation_util.cc',
    'notifier/invalidation_util.h',
    'notifier/unacked_invalidation_set.cc',
    'notifier/unacked_invalidation_set.h',
    'notifier/invalidator.h',
    'notifier/invalidator_registrar.cc',
    'notifier/invalidator_registrar.h',
    'notifier/invalidator_state.cc',
    'notifier/invalidator_state.h',
    'notifier/mock_ack_handler.cc',
    'notifier/mock_ack_handler.h',
    'notifier/object_id_invalidation_map.cc',
    'notifier/object_id_invalidation_map.h',
    'notifier/single_object_invalidation_set.cc',
    'notifier/single_object_invalidation_set.h',
  ],
  'conditions': [
    ['OS != "android"', {
      'sources': [
        'notifier/gcm_network_channel.cc',
        'notifier/gcm_network_channel.h',
        'notifier/gcm_network_channel_delegate.h',
        'notifier/invalidation_notifier.cc',
        'notifier/invalidation_notifier.h',
        'notifier/non_blocking_invalidator.cc',
        'notifier/non_blocking_invalidator.h',
        'notifier/p2p_invalidator.cc',
        'notifier/p2p_invalidator.h',
        'notifier/push_client_channel.cc',
        'notifier/push_client_channel.h',
        'notifier/registration_manager.cc',
        'notifier/registration_manager.h',
        'notifier/state_writer.h',
        'notifier/sync_invalidation_listener.cc',
        'notifier/sync_invalidation_listener.h',
        'notifier/sync_system_resources.cc',
        'notifier/sync_system_resources.h',
      ],
    }],
  ],
}
