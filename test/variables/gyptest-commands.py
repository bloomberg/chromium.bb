#!/usr/bin/env python

"""
Test variable expansion of '<!()' syntax commands.
"""

import TestGyp

test = TestGyp.TestGyp(format='gypd')

expect = """\
GENERAL: running with these options:
GENERAL:   msvs_version: None
GENERAL:   suffix: ''
GENERAL:   includes: None
GENERAL:   depth: '.'
GENERAL:   generator_flags: []
GENERAL:   formats: ['gypd']
GENERAL:   debug: ['variables', 'general']
GENERAL:   defines: None
VARIABLES: Expanding 'import math; print math.pi' to 'import math; print math.pi'
VARIABLES: Expanding 'ABCD' to 'ABCD'
VARIABLES: Expanding 'letters_' to 'letters_'
VARIABLES: Expanding 'python -c "print 'letters_list'"' to 'python -c "print 'letters_list'"'
VARIABLES: Executing command 'python -c "print 'letters_list'"'
VARIABLES: Command Replacement 'letters_list' (0, 36)
VARIABLES: Building '' + 'letters_list' + ''
VARIABLES: Expanding 'letters_list' to 'letters_list'
VARIABLES: Building '' + 'ABCD' + ''
VARIABLES: Expanding 'ABCD' to 'ABCD'
VARIABLES: Expanding 'list' to 'list'
VARIABLES: Expanding 'pi' to 'pi'
VARIABLES: Building '["python", "-c", "' + 'import math; print math.pi' + '"]'
VARIABLES: Expanding '["python", "-c", "import math; print math.pi"]' to '["python", "-c", "import math; print math.pi"]'
VARIABLES: Executing command '['python', '-c', 'import math; print math.pi']'
VARIABLES: Command Replacement '3.14159265359' (0, 29)
VARIABLES: Building '' + '3.14159265359' + ''
VARIABLES: Expanding '3.14159265359' to '3.14159265359'
VARIABLES: Expanding 'letters_list' to 'letters_list'
VARIABLES: Building 'python -c "print '' + 'ABCD' + ''"'
VARIABLES: Expanding 'python -c "print 'ABCD'"' to 'python -c "print 'ABCD'"'
VARIABLES: Executing command 'python -c "print 'ABCD'"'
VARIABLES: Command Replacement 'ABCD' (0, 39)
VARIABLES: Building '' + 'ABCD' + ''
VARIABLES: Expanding 'ABCD' to 'ABCD'
VARIABLES: Expanding 'letters_list' to 'letters_list'
VARIABLES: Building '<!(python -c "print '<!(python -c "<(pi)") ' + 'ABCD' + ''")'
VARIABLES: Expanding 'pi' to 'pi'
VARIABLES: Building 'python -c "' + 'import math; print math.pi' + '"'
VARIABLES: Expanding 'python -c "import math; print math.pi"' to 'python -c "import math; print math.pi"'
VARIABLES: Executing command 'python -c "import math; print math.pi"'
VARIABLES: Command Replacement '3.14159265359' (18, 39)
VARIABLES: Building 'python -c "print '' + '3.14159265359' + ' ABCD'"'
VARIABLES: Expanding 'python -c "print '3.14159265359 ABCD'"' to 'python -c "print '3.14159265359 ABCD'"'
VARIABLES: Executing command 'python -c "print '3.14159265359 ABCD'"'
VARIABLES: Command Replacement '3.14159265359 ABCD' (0, 50)
VARIABLES: Building '' + '3.14159265359 ABCD' + ''
VARIABLES: Expanding '3.14159265359 ABCD' to '3.14159265359 ABCD'
VARIABLES: Expanding 'foo' to 'foo'
VARIABLES: Expanding 'none' to 'none'
VARIABLES: Expanding 'var6' to 'var6'
VARIABLES: Building '<!(echo <(var5)' + 'list' + ')'
VARIABLES: Expanding 'var5' to 'var5'
VARIABLES: Building 'echo ' + 'letters_' + 'list'
VARIABLES: Expanding 'echo letters_list' to 'echo letters_list'
VARIABLES: Executing command 'echo letters_list'
VARIABLES: Command Replacement 'letters_list' (0, 20)
VARIABLES: Building '' + 'letters_list' + ''
VARIABLES: Expanding 'letters_list' to 'letters_list'
VARIABLES: Expanding 'test_action' to 'test_action'
VARIABLES: Expanding 'echo' to 'echo'
VARIABLES: Expanding '_inputs' to '_inputs'
VARIABLES: Expanding 'var2' to 'var2'
VARIABLES: Building '' + '3.14159265359 ABCD' + ''
VARIABLES: Expanding '3.14159265359 ABCD' to '3.14159265359 ABCD'
VARIABLES: Building '' + '"3.14159265359 ABCD"' + ''
VARIABLES: Expanding '"3.14159265359 ABCD"' to '"3.14159265359 ABCD"'
VARIABLES: Expanding '_outputs' to '_outputs'
VARIABLES: Expanding 'var4' to 'var4'
VARIABLES: Building '' + 'ABCD' + ''
VARIABLES: Expanding 'ABCD' to 'ABCD'
VARIABLES: Expanding 'var7' to 'var7'
VARIABLES: Building '' + 'letters_list' + ''
VARIABLES: Expanding 'letters_list' to 'letters_list'
VARIABLES: Building '' + 'ABCD letters_list' + ''
VARIABLES: Expanding 'ABCD letters_list' to 'ABCD letters_list'
VARIABLES: Expanding '3.14159265359 ABCD' to '3.14159265359 ABCD'
VARIABLES: Expanding 'ABCD' to 'ABCD'
VARIABLES: Expanding 'letters_list' to 'letters_list'
VARIABLES: Expanding 'commands.gyp' to 'commands.gyp'
VARIABLES: Expanding 'letters_' to 'letters_'
VARIABLES: Expanding 'ABCD' to 'ABCD'
VARIABLES: Expanding 'list' to 'list'
VARIABLES: Expanding '3.14159265359' to '3.14159265359'
VARIABLES: Expanding 'ABCD' to 'ABCD'
VARIABLES: Expanding '3.14159265359 ABCD' to '3.14159265359 ABCD'
VARIABLES: Expanding 'foo' to 'foo'
VARIABLES: Expanding 'none' to 'none'
VARIABLES: Expanding 'letters_list' to 'letters_list'
VARIABLES: Expanding 'test_action' to 'test_action'
VARIABLES: Expanding 'echo' to 'echo'
VARIABLES: Expanding '"3.14159265359 ABCD"' to '"3.14159265359 ABCD"'
VARIABLES: Expanding 'ABCD letters_list' to 'ABCD letters_list'
VARIABLES: Expanding '3.14159265359 ABCD' to '3.14159265359 ABCD'
VARIABLES: Expanding 'ABCD' to 'ABCD'
VARIABLES: Expanding 'letters_list' to 'letters_list'
"""

test.run_gyp('commands.gyp',
             '--debug', 'variables', '--debug', 'general',
             stdout=expect)

test.must_match('commands.gypd', test.read('commands.gypd.golden'))

test.pass_test()
