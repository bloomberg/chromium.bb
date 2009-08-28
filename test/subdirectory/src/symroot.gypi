{
  'variables': {
    'set_symroot%': 0,
  },
  'conditions': [
    ['set_symroot == 1', {
      'xcode_settings': {
        'SYMROOT': '<(DEPTH)/build',
      },
    }],
  ],
}
