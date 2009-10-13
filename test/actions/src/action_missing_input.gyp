{
  'targets': [
    {
      'target_name': 'broken_actions1',
      'type': 'none',
      'actions': [
        {
          'action_name': 'missing_inputs',
          'action': [
            'python',
            '-c',
            'print \'missing inputs\'',
          ],
        },
      ],
    },
  ],
}
