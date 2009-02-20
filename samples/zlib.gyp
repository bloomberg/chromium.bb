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
        'contrib/minizip/ioapi.c',
        'contrib/minizip/ioapi.h',
        'contrib/minizip/iowin32.c'
        'contrib/minizip/iowin32.h',
        'contrib/minizip/unzip.c',
        'contrib/minizip/unzip.h',
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
      'include_dirs': [
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'conditions': [
        [ 'OS!="win"', {
            'sources/': [['exclude', '^contrib/minizip/']]
          }
        ],
      ],
    },
  ],
}
