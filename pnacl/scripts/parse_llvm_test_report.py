#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parse the report output of the llvm test suite or regression tests,
   filter out known failures, and check for new failures

pnacl/scripts/parse_llvm_test_report.py [options]+ reportfile

"""

import csv
import optparse
import os
import sys

# exclude these tests
EXCLUDES = {}

def ParseCommandLine(argv):
  parser = optparse.OptionParser()
  parser.add_option('-x', '--exclude', action='append', dest='excludes',
                    default=[],
                    help='Add list of excluded tests (expected fails)')
  parser.add_option('-c', '--check-excludes', action='store_true',
                    default=False, dest='check_excludes',
                    help='Report tests which unexpectedly pass')
  parser.add_option('-v', '--verbose', action='store_true',
                    default=False, dest='verbose',
                    help='Print compilation/run logs of failing tests')
  parser.add_option('-p', '--build-path', dest='buildpath',
                    help='Path to test-suite build directory')
  parser.add_option('-a', '--attribute', dest='attributes', action='append',
                    default=[],
                    help='Add attribute of test configuration (e.g. arch)')
  parser.add_option('-t', '--testsuite', action='store_true', dest='testsuite',
                    default=False)
  parser.add_option('-l', '--lit', action='store_true', dest='lit',
                    default=False)

  options, args = parser.parse_args(argv)
  return options, args

def Fatal(text):
  print >> sys.stderr, text
  sys.exit(1)

def IsFullname(name):
  return name.find('/') != -1

def GetShortname(fullname):
  return fullname.split('/')[-1]

def ParseTestsuiteCSV(filename):
  ''' Parse a CSV file output by llvm testsuite with a record for each test.
      returns 2 dictionaries:
      1) a mapping from the short name of the test (without the path) to
       a list of full pathnames that match it. It contains all the tests.
      2) a mapping of all test failures, mapping full test path to the type
       of failure (compile or exec)
  '''
  alltests = {}
  failures = {}
  reader = csv.DictReader(open(filename, 'rb'))

  testcount = 0
  for row in reader:
    testcount += 1
    fullname = row['Program']
    shortname = GetShortname(fullname)
    fullnames = alltests.get(shortname, [])
    fullnames.append(fullname)
    alltests[shortname] = fullnames

    if row['CC'] == '*':
      failures[fullname] = 'compile'
    elif row['Exec'] == '*':
      failures[fullname] = 'exec'

  print testcount, 'tests,', len(failures), 'failures'
  return alltests, failures

def ParseLit(filename):
  ''' Parse the output of the LLVM regression test runner (lit/make check).
      returns a dictionary mapping test name to the type of failure
      (Clang, LLVM, LLVMUnit, etc)
  '''
  alltests = {}
  failures = {}
  testcount = 0
  with open(filename) as f:
    for line in f:
      l = line.split()
      if len(l) < 4:
        continue
      if l[0] in ('PASS:', 'FAIL:', 'XFAIL:', 'XPASS:', 'UNSUPPORTED:'):
        testcount += 1
        fullname = ''.join(l[1:4])
        shortname = GetShortname(fullname)
        fullnames = alltests.get(shortname, [])
        fullnames.append(fullname)
        alltests[shortname] = fullnames
      if l[0] in ('FAIL:', 'XPASS:'):
        failures[fullname] = l[1]
  print testcount, 'tests,', len(failures), 'failures'
  return alltests, failures

def ParseExcludeFile(filename, config_attributes,
                     check_test_names=False, alltests=None):
  ''' Parse a list of excludes (known test failures). Excludes can be specified
      by shortname (e.g. fbench) or by full path
      (e.g. SingleSource/Benchmarks/Misc/fbench) but if there is more than
      one test with the same shortname, the full name must be given.
      Errors are reported if an exclude does not match exactly one test
      in alltests, or if there are duplicate excludes.
  '''
  f = open(filename)
  for line in f:
    line = line.strip()
    if not line: continue
    if line.startswith('#'): continue
    tokens = line.split()
    if len(tokens) > 1:
      testname = tokens[0]
      attributes = set(tokens[1].split(','))
      if not attributes.issubset(config_attributes):
        continue
    else:
      testname = line
    if testname in EXCLUDES:
      Fatal('ERROR: duplicate exclude: ' + line)
    if IsFullname(testname):
      shortname = GetShortname(testname)
      if shortname not in alltests or testname not in alltests[shortname]:
        Fatal('ERROR: exclude ' + line + ' not found in list of tests')
      fullname = testname
    else:
      # short name is specified
      shortname = testname
      if shortname not in alltests:
        Fatal('ERROR: exclude ' + shortname + ' not found in list of tests')
      if len(alltests[shortname]) > 1:
        Fatal('ERROR: exclude ' + shortname + ' matches more than one test: ' +
              str(alltests[shortname]) + '. Specify full name in exclude file.')
      fullname = alltests[shortname][0]

    if fullname in EXCLUDES:
      Fatal('ERROR: duplicate exclude ' + fullname)

    EXCLUDES[fullname] = filename
  f.close()
  print 'parsed', filename + ': now', len(EXCLUDES), 'total excludes'

def DumpFileContents(name):
  print name
  print open(name, 'rb').read()

def PrintCompilationResult(path, test):
  ''' Print the compilation and run results for the specified test.
      These results are left in several different log files by the testsuite
      driver, and are different for MultiSource/SingleSource tests
  '''
  print 'RESULTS for', test
  testpath = os.path.join(path, test)
  testdir, testname = os.path.split(testpath)
  outputdir = os.path.join(testdir, 'Output')

  print 'COMPILE phase'
  print 'OBJECT file phase'
  if test.startswith('MultiSource'):
    for f in os.listdir(outputdir):
      if f.endswith('llvm.o.compile'):
        DumpFileContents(os.path.join(outputdir, f))
  elif test.startswith('SingleSource'):
    DumpFileContents(os.path.join(outputdir, testname + '.llvm.o.compile'))
  else:
    Fatal('ERROR: unrecognized test type ' + test)

  print 'PEXE generation phase'
  DumpFileContents(os.path.join(outputdir, testname + '.pexe.compile'))

  print 'TRANSLATION phase'
  DumpFileContents(os.path.join(outputdir, testname + '.nexe.translate'))

  print 'EXECUTION phase'
  print 'native output:'
  DumpFileContents(os.path.join(outputdir, testname + '.out-nat'))
  print 'pnacl output:'
  DumpFileContents(os.path.join(outputdir, testname + '.out-pnacl'))

def main(argv):
  options, args = ParseCommandLine(argv[1:])

  if len(args) != 1:
    Fatal('Must specify filename to parse')
  if options.verbose:
    if options.buildpath is None:
      Fatal('ERROR: must specify build path if verbose output is desired')

  filename = args[0]
  failures = {}
  if options.verbose:
    print 'Full test results:'

  # get the set of tests and failures
  if options.testsuite:
    alltests, failures = ParseTestsuiteCSV(filename)
    check_test_names = True
  elif options.lit:
    alltests, failures = ParseLit(filename)
    check_test_names = True
  else:
    Fatal('Must specify either testsuite (-t) or lit (-l) output format')

  # get the set of excludes
  for f in options.excludes:
    ParseExcludeFile(f, set(options.attributes),
                     check_test_names=check_test_names, alltests=alltests)

  # intersect them and check for unexpected fails/passes
  unexpected_failures = 0
  unexpected_passes = 0
  for tests in alltests.itervalues():
    for test in tests:
      if test in failures:
        if test not in EXCLUDES:
          unexpected_failures += 1
          print '[  FAILED  ] ' + test + ': ' + failures[test] + ' failure'
          if options.verbose:
            PrintCompilationResult(options.buildpath, test)
      elif test in EXCLUDES:
        unexpected_passes += 1
        print test + ': ' + ' unexpected success'

  print unexpected_failures, 'unexpected failures',
  print unexpected_passes, 'unexpected passes'

  if options.check_excludes:
    return unexpected_failures + unexpected_passes > 0
  return unexpected_failures > 0

if __name__ == '__main__':
  sys.exit(main(sys.argv))
