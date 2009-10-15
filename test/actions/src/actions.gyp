{
  'targets': [
    {
      'target_name': 'pull_in_all_actions',
      'type': 'none',
      'dependencies': [
        'subdir1/executable.gyp:*',
        'subdir2/none.gyp:*',
        'subdir3/null_input.gyp:*',
      ],
    },
  ],
}
