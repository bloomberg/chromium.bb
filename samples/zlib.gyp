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
      'target_name': 'zlib',
      'type': 'static_library',
      'sources': [
        'adler32.c',
        'compress.c',
        'crc32.c',
        'crc32.h',
        'deflate.c',
        'deflate.h',
        'gzio.c',
        'infback.c',
        'inffast.c',
        'inffast.h',
        'inffixed.h',
        'inflate.c',
        'inflate.h',
        'inftrees.c',
        'inftrees.h',
        'mozzconf.h',
        'trees.c',
        'trees.h',
        'uncompr.c',
        'zconf.h',
        'zlib.h',
        'zutil.c',
        'zutil.h',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
    },
  ],
}
