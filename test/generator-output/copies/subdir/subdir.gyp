{
  'targets': [
    {
      'target_name': 'copies3',
      'type': 'none',
      'copies': [
        {
          'destination': 'copies-out',
          'files': [
            'file3',
          ],
        },
      ],
    },
    {
      'target_name': 'copies4',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/copies-out',
          'files': [
            'file4',
          ],
        },
      ],
    },
  ],
}
