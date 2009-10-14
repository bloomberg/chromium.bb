{
  'targets': [
    {
      'target_name': 'broken_actions2',
      'type': 'none',
      'actions': [
        {
          'inputs': [
            'no_name.input',
          ],
          'action': [
            'python',
            '-c',
            'print \'missing name\'',
          ],
        },
      ],
    },
  ],
}
