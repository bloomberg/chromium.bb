{
  'targets': [
    {
      'target_name': 'system_api-protos',
      'type': 'none',
      'variables': {
        'proto_in_dir': 'dbus',
        'proto_out_dir': 'include/chromeos/dbus',
      },
      'cflags': [
        '-fvisibility=hidden',
      ],
      'sources': [
        '<(proto_in_dir)/mtp_storage_info.proto',
        '<(proto_in_dir)/mtp_file_entry.proto',
        '<(proto_in_dir)/video_activity_update.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'system_api-power_manager-protos',
      'type': 'none',
      'variables': {
        'proto_in_dir': 'dbus/power_manager',
        'proto_out_dir': 'include/chromeos/dbus/power_manager',
      },
      'cflags': [
        '-fvisibility=hidden',
      ],
      'sources': [
        '<(proto_in_dir)/suspend.proto',
        '<(proto_in_dir)/input_event.proto',
        '<(proto_in_dir)/policy.proto',
        '<(proto_in_dir)/power_supply_properties.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'system_api-headers',
      'type': 'none',
      'copies': [
        {
          'destination': '<(SHARED_INTERMEDIATE_DIR)/include/chromeos/dbus',
          'files': [
            'dbus/service_constants.h'
          ]
        }
      ]
    }
  ]
}
