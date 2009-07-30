#!/usr/bin/env python

import difflib
import os
import subprocess
import sys

# Returns file contents as a list of lines.
def GetFileContents(filename, test_name):
  try:
    fd = open(filename, "r")
  except IOError:
    print "Unable to open %s for test %s" % (filename, test_name)
    return False
  output = fd.read().splitlines()
  fd.close()
  return output

def RunTest(test_name, golden_file):
  gyp_script = os.path.normpath(
    os.path.join(os.path.dirname(sys.argv[0]), '../gyp'))

  p = subprocess.Popen(['python', gyp_script, '--depth', '..', test_name,
                        '--debug', 'variables', '--debug', 'general',
                        '--format', 'gypd'],
                       shell=(sys.platform == 'win32'), stdin=subprocess.PIPE,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)

  (p_stdout, p_stderr) = p.communicate('')
  if p_stderr:
    print "Test %s FAILED to execute: with error output" % (test_name)
    print "Error output was:\n%s" % p_stdout
    print "Output was:\n%s" % p_stdout
    return False

  p_stdout = p_stdout.splitlines()

  golden = GetFileContents(golden_file, test_name)

  if golden != p_stdout:
    print "Test %s FAILED: output doesn't match golden file %s" % (test_name,
                                                                   golden_file)
    print "Difference is:"
    for d in difflib.context_diff(p_stdout, golden):
      print d
    return False

  # Now we check to see that the gypd output matches the golden file.
  golden = GetFileContents(test_name + 'd.golden', test_name)
  gypd = GetFileContents(test_name + 'd', test_name)

  if golden != gypd:
    print 'Test %s FAILED: GYPD output ' \
          'does not match golden file (%s)' % (test_name, golden_file)
    print "Difference is:"
    for d in difflib.context_diff(gypd, golden):
      print d
    return False

  return True

if __name__ == '__main__':
  tests = [
    ('test1/test1.gyp', 'test1.golden'),
    ]
  failures = 0
  for test in tests:
    if not RunTest(test[0], test[1]):
      failures = failures + 1
  if failures > 0:
    fail_plural = ""
    if failures != 1:
      fail_plural = "s"
    print "%d test%s FAILED" % (failures, fail_plural)
    successes = len(tests) - failures
    success_plural = ""
    if successes != 1:
      success_plural = "s"
    print "%d test%s SUCCEEDED" % (successes, success_plural)
    sys.exit(2)
  else:
    print "ALL %d tests SUCCEEDED" % len(tests)
    sys.exit(0)
