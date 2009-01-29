{
  'targets': [
    {
      'name': 'libpng',
      'type': 'static_library',
      'dependencies': [
        '../zlib/zlib.gyp:zlib',
      ],
      'sources': [
        'png.c',
        'png.h',
        'pngconf.h',
        'pngerror.c',
        'pnggccrd.c',
        'pngget.c',
        'pngmem.c',
        'pngpread.c',
        'pngread.c',
        'pngrio.c',
        'pngrtran.c',
        'pngrutil.c',
        'pngset.c',
        'pngtrans.c',
        'pngusr.h',
        'pngvcrd.c',
        'pngwio.c',
        'pngwrite.c',
        'pngwtran.c',
        'pngwutil.c',
      ],
      'vs_props': [ '../../build/external_code.vsprops' ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'PNG_USER_CONFIG',
        'CHROME_PNG_WRITE_SUPPORT',
      ],
      'dependent_settings': {
        'include_dirs': ['..'],
        'defines': [
          'PNG_USER_CONFIG',
          'CHROME_PNG_WRITE_SUPPORT',
        ],
      },
    }
  ]
}
