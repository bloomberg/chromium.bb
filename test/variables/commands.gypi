# This file is included from commands.gyp to test evaluation order of includes.
{
  'variables': {
    'included_variable': 'XYZ',
  },
  'targets': [
    {
      'target_name': 'dummy',
      'type': 'none',
    },
  ],
}
