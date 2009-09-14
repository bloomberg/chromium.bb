{
  'variables': {
    'value%': '5',
  },
  'targets': [
    {
      'target_name': 'defines',
      'type': 'executable',
      'sources': [
        'defines.c',
      ],
      'defines': [
        'VALUE=<(value)',
      ],
    },
  ],
}

