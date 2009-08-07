# This is a simple test file to make sure that variable substitution
# happens correctly.  Run "run_tests.py" using python to generate the
# output from this gyp file.

{
  'variables': {
    'pi': 'import math; print math.pi',
    'letters_list': 'ABCD',
  },
  'targets': [
    {
      'target_name': 'foo',
      'type': 'none',
      'variables': {
        'var1': '<!(["python", "-c", "<(pi)"])',
        'var2': '<!(python -c "print \'<!(python -c "<(pi)") <(letters_list)\'")',
        'var3': '<!(python -c "print \'<(letters_list)\'")',
        'var4': '<(<!(python -c "print \'letters_list\'"))',
        'var5': 'letters_',
        'var6': 'list',
      },
      'actions': [
        {
          'action_name': 'test_action',
          'variables': {
            'var7': '<!(echo <(var5)<(var6))',
          },
          'inputs' : [
            '<(var2)',
          ],
          'outputs': [
            '<(var4)',
            '<(var7)',
          ],
          'action': [
            'echo',
            '<(_inputs)',
            '<(_outputs)',
          ],
        },
      ],
    },
  ],
}
