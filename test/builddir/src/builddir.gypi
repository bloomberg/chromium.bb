{
  'target_defaults': {
    'configurations': {
      'Default': {
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(DEPTH)\\builddir\Default',
        },
      },
    },
  },
  'scons_settings': {
    'sconsbuild_dir': '<(DEPTH)/builddir',
  },
  'xcode_settings': {
    'SYMROOT': '<(DEPTH)/builddir',
  },
}
