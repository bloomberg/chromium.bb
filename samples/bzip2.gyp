{
  'variables': {
    'depth': '../..',
  },
  'includes': [
    '../../build/common.gypi',
    '../../build/external_code.gypi',
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
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
    },
  ],
}
