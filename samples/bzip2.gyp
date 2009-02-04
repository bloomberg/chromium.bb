{
  'variables': {
    'depth': '../..',
  },
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'bzip2',
      'type': 'static_library',
      'defines': ['BZ_NO_STDIO'],
      'sources': [
        'blocksort.c',
        'bzlib.c',
        'bzlib.h',
        'bzlib_private.h',
        'compress.c',
        'crctable.c',
        'decompress.c',
        'huffman.c',
        'randtable.c',
      ],
      'vs_props': [ '../../build/external_code.vsprops' ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
    },
  ],
}
