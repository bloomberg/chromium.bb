# This is a simple test file to make sure that variable substitution
# happens correctly.  Run "run_tests.py" using python to generate the
# output from this gyp file.

{
  'variables': {
    'pi': 'import math; print math.pi',
    'letters': 'ABCD',
  },
  'targets': [
    {
      'target_name': 'foo',
      'type': 'none',
      'variables': {
        'var1': '<!(["python", "-c", "<(pi)"])',
        'var2': '<!(python -c "print \'<!(python -c "<(pi)") <(letters)\'")',
        'var3': '<!(python -c "print \'<(letters)\'")',
        'var4': '<(<!(python -c "print \'letters\'"))',
      },
      'actions': [
        {
          'action_name': 'test_action',
          'inputs' : [
            '<(var2)',
          ],
          'outputs': [
            '<(var4)',
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
