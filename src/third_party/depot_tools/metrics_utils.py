#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import re
import scm
import subprocess2
import sys
import urlparse

from third_party import colorama


# Current version of metrics recording.
# When we add new metrics, the version number will be increased, we display the
# user what has changed, and ask the user to agree again.
CURRENT_VERSION = 1

APP_URL = 'https://cit-cli-metrics.appspot.com'

EMPTY_LINE = (
  '*                                                   *'
)
NOTICE_COUNTDOWN_HEADER = (
  '*****************************************************\n'
  '*  METRICS COLLECTION WILL START IN %2d EXECUTIONS   *'
)
NOTICE_COLLECTION_HEADER = (
  '*****************************************************\n'
  '*      METRICS COLLECTION IS TAKING PLACE           *'
)
NOTICE_VERSION_CHANGE_HEADER = (
  '*****************************************************\n'
  '*       WE ARE COLLECTING ADDITIONAL METRICS        *\n'
  '*                                                   *\n'
  '* Please review the changes and opt-in again.       *'
)
NOTICE_FOOTER = (
  '* To suppress this message opt in or out using:     *\n'
  '* $ gclient metrics [--opt-in] [--opt-out]          *\n'
  '* For more information please see metrics.README.md *\n'
  '* in your depot_tools checkout or visit             *\n'
  '* https://bit.ly/2ufRS4p.                           *\n'
  '*****************************************************\n'
)

CHANGE_NOTICE = {
  # No changes for version 0
  0: '',
  1: ('* We want to collect the Git version.               *\n'
      '* We want to collect information about the HTTP     *\n'
      '* requests that depot_tools makes, and the git and  *\n'
      '* cipd commands it executes.                        *\n'
      '*                                                   *\n'
      '* We only collect known strings to make sure we     *\n'
      '* don\'t record PII.                                 *')
}


KNOWN_PROJECT_URLS = {
  'https://chrome-internal.googlesource.com/chrome/ios_internal',
  'https://chrome-internal.googlesource.com/infra/infra_internal',
  'https://chromium.googlesource.com/breakpad/breakpad',
  'https://chromium.googlesource.com/chromium/src',
  'https://chromium.googlesource.com/chromium/tools/depot_tools',
  'https://chromium.googlesource.com/crashpad/crashpad',
  'https://chromium.googlesource.com/external/gyp',
  'https://chromium.googlesource.com/external/naclports',
  'https://chromium.googlesource.com/infra/goma/client',
  'https://chromium.googlesource.com/infra/infra',
  'https://chromium.googlesource.com/native_client/',
  'https://chromium.googlesource.com/syzygy',
  'https://chromium.googlesource.com/v8/v8',
  'https://dart.googlesource.com/sdk',
  'https://pdfium.googlesource.com/pdfium',
  'https://skia.googlesource.com/buildbot',
  'https://skia.googlesource.com/skia',
  'https://webrtc.googlesource.com/src',
}

KNOWN_HTTP_HOSTS = {
  'chrome-internal-review.googlesource.com',
  'chromium-review.googlesource.com',
  'dart-review.googlesource.com',
  'eu1-mirror-chromium-review.googlesource.com',
  'pdfium-review.googlesource.com',
  'skia-review.googlesource.com',
  'us1-mirror-chromium-review.googlesource.com',
  'us2-mirror-chromium-review.googlesource.com',
  'us3-mirror-chromium-review.googlesource.com',
  'webrtc-review.googlesource.com',
}

KNOWN_HTTP_METHODS = {
  'DELETE',
  'GET',
  'PATCH',
  'POST',
  'PUT',
}

KNOWN_HTTP_PATHS = {
  'accounts':
      re.compile(r'(/a)?/accounts/.*'),
  'changes':
      re.compile(r'(/a)?/changes/([^/]+)?$'),
  'changes/abandon':
      re.compile(r'(/a)?/changes/.*/abandon'),
  'changes/comments':
      re.compile(r'(/a)?/changes/.*/comments'),
  'changes/detail':
      re.compile(r'(/a)?/changes/.*/detail'),
  'changes/edit':
      re.compile(r'(/a)?/changes/.*/edit'),
  'changes/message':
      re.compile(r'(/a)?/changes/.*/message'),
  'changes/restore':
      re.compile(r'(/a)?/changes/.*/restore'),
  'changes/reviewers':
      re.compile(r'(/a)?/changes/.*/reviewers/.*'),
  'changes/revisions/commit':
      re.compile(r'(/a)?/changes/.*/revisions/.*/commit'),
  'changes/revisions/review':
      re.compile(r'(/a)?/changes/.*/revisions/.*/review'),
  'changes/submit':
      re.compile(r'(/a)?/changes/.*/submit'),
  'projects/branches':
      re.compile(r'(/a)?/projects/.*/branches/.*'),
}

KNOWN_HTTP_ARGS = {
  'ALL_REVISIONS',
  'CURRENT_COMMIT',
  'CURRENT_REVISION',
  'DETAILED_ACCOUNTS',
  'LABELS',
}

GIT_VERSION_RE = re.compile(
  r'git version (\d)\.(\d{0,2})\.(\d{0,2})'
)

KNOWN_SUBCOMMAND_ARGS = {
  'cc',
  'hashtag',
  'l=Auto-Submit+1',
  'l=Commit-Queue+1',
  'l=Commit-Queue+2',
  'label',
  'm',
  'notify=ALL',
  'notify=NONE',
  'private',
  'r',
  'ready',
  'topic',
  'wip'
}


def get_python_version():
  """Return the python version in the major.minor.micro format."""
  return '{v.major}.{v.minor}.{v.micro}'.format(v=sys.version_info)


def get_git_version():
  """Return the Git version in the major.minor.micro format."""
  p = subprocess2.Popen(
      ['git', '--version'],
      stdout=subprocess2.PIPE, stderr=subprocess2.PIPE)
  stdout, _ = p.communicate()
  match = GIT_VERSION_RE.match(stdout)
  if not match:
    return None
  return '%s.%s.%s' % match.groups()


def return_code_from_exception(exception):
  """Returns the exit code that would result of raising the exception."""
  if exception is None:
    return 0
  if isinstance(exception[1], SystemExit):
    return exception[1].code
  return 1


def seconds_to_weeks(duration):
  """Transform a |duration| from seconds to weeks approximately.

  Drops the lowest 19 bits of the integer representation, which ammounts to
  about 6 days.
  """
  return int(duration) >> 19


def extract_known_subcommand_args(args):
  """Extract the known arguments from the passed list of args."""
  known_args = []
  for arg in args:
    if arg in KNOWN_SUBCOMMAND_ARGS:
      known_args.append(arg)
    else:
      arg = arg.split('=')[0]
      if arg in KNOWN_SUBCOMMAND_ARGS:
        known_args.append(arg)
  return sorted(known_args)


def extract_http_metrics(request_uri, method, status, response_time):
  """Extract metrics from the request URI.

  Extracts the host, path, and arguments from the request URI, and returns them
  along with the method, status and response time.

  The host, method, path and arguments must be in the KNOWN_HTTP_* constants
  defined above.

  Arguments are the values of the o= url parameter. In Gerrit, additional fields
  can be obtained by adding o parameters, each option requires more database
  lookups and slows down the query response time to the client, so we make an
  effort to collect them.

  The regex defined in KNOWN_HTTP_PATH_RES are checked against the path, and
  those that match will be returned.
  """
  http_metrics = {
    'status': status,
    'response_time': response_time,
  }

  if method in KNOWN_HTTP_METHODS:
    http_metrics['method'] = method

  parsed_url = urlparse.urlparse(request_uri)

  if parsed_url.netloc in KNOWN_HTTP_HOSTS:
    http_metrics['host'] = parsed_url.netloc

  for name, path_re in KNOWN_HTTP_PATHS.iteritems():
    if path_re.match(parsed_url.path):
      http_metrics['path'] = name
      break

  parsed_query = urlparse.parse_qs(parsed_url.query)

  # Collect o-parameters from the request.
  args = [
    arg for arg in parsed_query.get('o', [])
    if arg in KNOWN_HTTP_ARGS
  ]
  if args:
    http_metrics['arguments'] = args

  return http_metrics


def get_repo_timestamp(path_to_repo):
  """Get an approximate timestamp for the upstream of |path_to_repo|.

  Returns the top two bits of the timestamp of the HEAD for the upstream of the
  branch path_to_repo is checked out at.
  """
  # Get the upstream for the current branch. If we're not in a branch, fallback
  # to HEAD.
  try:
    upstream = scm.GIT.GetUpstreamBranch(path_to_repo)
  except subprocess2.CalledProcessError:
    upstream = 'HEAD'

  # Get the timestamp of the HEAD for the upstream of the current branch.
  p = subprocess2.Popen(
      ['git', '-C', path_to_repo, 'log', '-n1', upstream, '--format=%at'],
      stdout=subprocess2.PIPE, stderr=subprocess2.PIPE)
  stdout, _ = p.communicate()

  # If there was an error, give up.
  if p.returncode != 0:
    return None

  # Get the age of the checkout in weeks.
  return seconds_to_weeks(stdout.strip())


def print_notice(countdown):
  """Print a notice to let the user know the status of metrics collection."""
  colorama.init()
  print(colorama.Fore.RED + '\033[1m', file=sys.stderr, end='')
  if countdown:
    print(NOTICE_COUNTDOWN_HEADER % countdown, file=sys.stderr)
  else:
    print(NOTICE_COLLECTION_HEADER, file=sys.stderr)
  print(EMPTY_LINE, file=sys.stderr)
  print(NOTICE_FOOTER + colorama.Style.RESET_ALL, file=sys.stderr)


def print_version_change(config_version):
  """Print a notice to let the user know we are collecting more metrics."""
  colorama.init()
  print(colorama.Fore.RED + '\033[1m', file=sys.stderr, end='')
  print(NOTICE_VERSION_CHANGE_HEADER, file=sys.stderr)
  print(EMPTY_LINE, file=sys.stderr)
  for version in range(config_version + 1, CURRENT_VERSION + 1):
    print(CHANGE_NOTICE[version], file=sys.stderr)
    print(EMPTY_LINE, file=sys.stderr)
