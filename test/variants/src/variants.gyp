{
  'targets': [
    {
      'target_name': 'variants',
      'type': 'executable',
      'sources': [
        'variants.c',
      ],
      'variants': {
        'variant1' : {
          'defines': [
            'VARIANT1',
          ],
        },
        'variant2' : {
          'defines': [
            'VARIANT2',
          ],
        },
      },
    },
  ],
}
