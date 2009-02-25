{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'npapi',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          # Some things #include "bindings/npapi.h" and others just #include
          # "npapi.h".  Account for both flavors.
          '.',
          'bindings',
        ],
      },
    },
  ],
}
