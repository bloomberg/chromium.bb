# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import fnmatch
import os
import sys
import traceback
import unittest

def Discover(start_dir, pattern = "test*.py", top_level_dir = None):
  if hasattr(unittest.defaultTestLoader, 'discover'):
    return unittest.defaultTestLoader.discover(start_dir,
                                               pattern,
                                               top_level_dir)

  # TODO(nduca): Do something with top_level_dir non-None
  modules = []
  for (dirpath, dirnames, filenames) in os.walk(start_dir):
    for filename in filenames:
      if not filename.endswith(".py"):
        continue

      if not fnmatch.fnmatch(filename, pattern):
        continue

      if filename.startswith('.') or filename.startswith('_'):
        continue
      name,ext = os.path.splitext(filename)
      fqn = dirpath.replace('/', '.') + '.' + name

      # load the module
      try:
        module = __import__(fqn, fromlist=[True])
      except:
        print "While importing [%s]\n" % fqn
        traceback.print_exc()
        continue
      modules.append(module)

  loader = unittest.defaultTestLoader
  subsuites = []
  for module in modules:
    if hasattr(module, 'suite'):
      new_suite = module.suite()
    else:
      new_suite = loader.loadTestsFromModule(module)
    if new_suite.countTestCases():
      subsuites.append(new_suite)
  return unittest.TestSuite(subsuites)

def FilterSuite(suite, predicate):
  new_suite = unittest.TestSuite()
  for x in suite:
    if isinstance(x, unittest.TestSuite):
      subsuite = FilterSuite(x, predicate)
      if subsuite.countTestCases() == 0:
        continue

      new_suite.addTest(subsuite)
      continue

    assert isinstance(x, unittest.TestCase)
    if predicate(x):
      new_suite.addTest(x)

  return new_suite

'''Unit test suite that collects all test cases for chrome_remote_control.'''
def Main(argv):
  dir_name = os.path.join(os.path.dirname(__file__), "..")
  olddir = os.getcwd()
  try:
    os.chdir(dir_name)
    suite = Discover("chrome_remote_control", "*_unittest.py", ".")

    def IsTestSelected(test):
      if len(argv) == 1:
        return True
      for name in argv[1:]:
        if str(test).find(name) != -1:
          return True
      return False

    filtered_suite = FilterSuite(suite, IsTestSelected)
    runner = unittest.TextTestRunner(verbosity = 2)
    test_result = runner.run(filtered_suite)
    return len(test_result.errors) + len(test_result.failures)

  finally:
    os.chdir(olddir)
  return 1

if __name__ == "__main__":
  sys.exit(Main(sys.argv))
