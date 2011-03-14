#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Retrieve passing and failing WebKit revision numbers from canaries.

From each canary,
- the last WebKit revision number for which all the tests have passed,
- the last WebKit revision number for which the tests were run, and
- the names of failing layout tests
are retrieved and printed.
"""


import json
import optparse
import sys
import urllib2


_DEFAULT_BUILDERS = [
  "Webkit Win",
  "Webkit Mac10.5",
  "Webkit Linux",
]
_DEFAULT_MAX_BUILDS = 10
_TEST_PREFIX = "&tests="
_TEST_SUFFIX = '">'
_WEBKIT_TESTS = "webkit_tests"


class _BuildResult(object):
  """Build result for a builder.

  Holds builder name, the last passing revision, the last run revision, and
  a list of names of failing tests. Revision nubmer 0 is used to represent
  that the revision doesn't exist.
  """
  def __init__(self, builder, last_passing_revision, last_run_revision,
               failing_tests):
    """Constructs build results."""
    self.builder = builder
    self.last_passing_revision = last_passing_revision
    self.last_run_revision = last_run_revision
    self.failing_tests = failing_tests


def _BuilderUrlFor(builder, max_builds):
  """Constructs the URL for a builder to retrieve the last results."""
  return ("http://build.chromium.org/p/chromium.webkit/json/builders/" +
          urllib2.quote(builder) + "/builds?as_text=1&" +
          '&'.join(["select=%d" % -i for i in range(1, 1 + max_builds)]))


def _ExtractFailingTests(build):
  """Extracts failing test names from a build result entry JSON object."""
  for step in build["steps"]:
    if step["name"] == _WEBKIT_TESTS:
      for text in step["text"]:
        prefix = text.find(_TEST_PREFIX)
        suffix = text.find(_TEST_SUFFIX)
        if prefix != -1 and suffix != -1:
          return sorted(text[prefix + len(_TEST_PREFIX): suffix].split(","))


def _RetrieveBuildResult(builder, max_builds):
  """Retrieves build results for a builder.

  Checks the last passing revision, the last run revision, and failing tests
  for the last builds of a builder.

  Args:
      builder: Builder name.
      max_builds: Maximum number of builds to check.

  Returns:
      _BuildResult instance.
  """
  last_run_revision = 0
  failing_tests = []
  succeeded = False
  builds_json = urllib2.urlopen(_BuilderUrlFor(builder, max_builds))
  if not builds_json:
    return _BuildResult(builder, 0, 0, failing_tests)
  builds = [(int(value["number"]), value) for unused_key, value
            in json.loads(''.join(builds_json)).items()
            if value.has_key("number")]
  builds.sort()
  builds.reverse()
  for unused_key, build in builds:
    if not build.has_key("text"):
      continue
    if len(build["text"]) < 2:
      continue
    if not build.has_key("sourceStamp"):
      continue
    if build["text"][1] == "successful":
      succeeded = True
    elif not failing_tests and _WEBKIT_TESTS in build["text"][1:]:
      failing_tests = _ExtractFailingTests(build)
    revision = 0
    if build["sourceStamp"]["branch"] == "trunk":
      revision = int(build["sourceStamp"]["changes"][-1]["revision"])
    if revision and not last_run_revision:
      last_run_revision = revision
    if not succeeded or not revision:
      continue
    return _BuildResult(builder, revision, last_run_revision, failing_tests)
  return _BuildResult(builder, 0, last_run_revision, failing_tests)


def _PrintPassingRevisions(results):
  """Prints passing revisions and the range of such revisions.

  Args:
      results: A list of build results.
  """
  print "**** Passing revisions *****"
  minimum_revision = sys.maxint
  maximum_revision = 0
  for result in results:
    if result.last_passing_revision:
      minimum_revision = min(minimum_revision, result.last_passing_revision)
      maximum_revision = max(maximum_revision, result.last_passing_revision)
      print 'The last passing run was at r%d on "%s"' % (
          result.last_passing_revision, result.builder)
    else:
      print 'No passing runs on "%s"' % result.builder
  if maximum_revision:
    print "Passing revision range: r%d - r%d" % (
          minimum_revision, maximum_revision)


def _PrintFailingRevisions(results):
  """Prints failing revisions and the failing tests.

  Args:
      results: A list of build results.
  """
  print "**** Failing revisions *****"
  for result in results:
    if result.last_run_revision and result.failing_tests:
      print ('The last run was at r%d on "%s" and the following %d tests'
             ' failed' % (result.last_run_revision, result.builder,
                          len(result.failing_tests)))
      for test in result.failing_tests:
        print "  " + test


def _ParseOptions():
  """Parses command-line options."""
  parser = optparse.OptionParser(usage="%prog [options] [builders]")
  parser.add_option("-m", "--max_builds", type="int",
                    default=_DEFAULT_MAX_BUILDS,
                    help="maximum number of builds to check for each builder")
  return parser.parse_args()


def _Main():
  """The main function."""
  options, builders = _ParseOptions()
  if not builders:
    builders = _DEFAULT_BUILDERS
  print "Checking the last %d builds for:" % options.max_builds
  sys.stdout.flush()
  results = []
  for builder in builders:
    print '"%s"' % builder
    sys.stdout.flush()
    results.append(_RetrieveBuildResult(builder, options.max_builds))
  _PrintFailingRevisions(results)
  _PrintPassingRevisions(results)


if __name__ == "__main__":
  _Main()
