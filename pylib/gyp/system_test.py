#!/usr/bin/python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import shutil
import subprocess

def TestCommands(commands, files={}):
  """Run commands in a temporary directory, returning true if they all succeed.

  Arguments:
    commands: an array of argv lists, e.g. [['ls', '-l'], ['pwd']]
    files: a dictionary mapping filename to contents;
           files will be created in the temporary directory before running
           the command.
  """
  tempdir = tempfile.mkdtemp()
  try:
    for name, contents in files.items():
      f = open(os.path.join(tempdir, name), 'wb')
      f.write(contents)
      f.close()
    for command in commands:
      code = subprocess.call(command,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT,
                             cwd=tempdir)
      if code != 0:
        return False
    return True
  finally:
    shutil.rmtree(tempdir)
  return False


def TestArSupportsT(ar_command='ar', cc_command='cc'):
  """Test whether 'ar' supports the 'T' flag."""
  return TestCommands([[cc_command, '-c', 'test.c'],
                       [ar_command, 'crsT', 'test.a', 'test.o'],
                       [cc_command, 'test.a']],
                      files={'test.c': 'int main(){}'})


def TestLinkerSupportsThreads(cc_command='cc'):
  """Test whether the linker supports the --threads flag."""
  return TestCommands([[cc_command, '-Wl,--threads', 'test.c']],
                      files={'test.c': 'int main(){}'})


if __name__ == '__main__':
  # Run the various test functions and print the results.
  def RunTest(description, function):
    print "Testing " + description + ':',
    if function():
      print 'ok'
    else:
      print 'fail'
  RunTest("ar 'T' flag", TestArSupportsT)
  RunTest("ld --threads", TestLinkerSupportsThreads)
