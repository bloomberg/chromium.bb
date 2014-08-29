# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module contains functionality for starting build try jobs via HTTP.

This includes both sending a request to start a job, and also related code
for querying the status of the job.

This module can be either run as a stand-alone script to send a request to a
builder, or imported and used by calling the public functions below.
"""

import getpass
import json
import optparse
import os
import sys
import urllib
import urllib2

# URL template for fetching JSON data about builds.
BUILDER_JSON_URL = ('%(server_url)s/json/builders/%(bot_name)s/builds/'
                    '%(build_num)s?as_text=1&filter=0')

# URL template for displaying build steps.
BUILDER_HTML_URL = ('%(server_url)s/builders/%(bot_name)s/builds/%(build_num)s')

# Try server status page for the perf try server.
PERF_TRY_SERVER_URL = 'http://build.chromium.org/p/tryserver.chromium.perf'

# Hostname of the tryserver where perf bisect builders are hosted.
# This is used for posting build requests to the tryserver.
PERF_BISECT_BUILDER_HOST = 'master4.golo.chromium.org'

# The default 'try_job_port' on tryserver to post build request.
PERF_BISECT_BUILDER_PORT = 8341

# Status codes that can be returned by the GetBuildStatus method.
# From buildbot.status.builder.
# See: http://docs.buildbot.net/current/developer/results.html
SUCCESS, WARNINGS, FAILURE, SKIPPED, EXCEPTION, RETRY, TRYPENDING = range(7)

OK = (SUCCESS, WARNINGS)  # These indicate build is complete.
FAILED = (FAILURE, EXCEPTION, SKIPPED)  # These indicate build failure.
PENDING = (RETRY, TRYPENDING)  # These indicate in progress or in pending queue.


class ServerAccessError(Exception):

  def __str__(self):
    return '%s\nSorry, cannot connect to server.' % self.args[0]


def PostTryJob(host, port, params):
  """Sends a build request to the server using the HTTP protocol.

  The required parameters are:
    'revision': "src@rev", where rev is a git hash or SVN revision.
    'bot': Name of builder bot to use, e.g. "win_perf_bisect_builder".

  Args:
    host: Hostname of the try server.
    port: Port of the try server.
    params: A dictionary of parameters to be sent in the POST request.

  Returns:
    True if the request is posted successfully.

  Raises:
    ServerAccessError: Failed to make a request to the try server.
    ValueError: Not all of the expected inputs were given.
  """
  if not params.get('revision'):
    raise ValueError('Missing revision number.')
  if not params.get('bot'):
    raise ValueError('Missing bot name.')

  base_url = 'http://%s:%s/send_try_patch' % (host, port)
  print 'Posting build request by HTTP.'
  print 'URL: %s, params: %s' % (base_url, params)

  connection = None
  try:
    print 'Opening connection...'
    connection = urllib2.urlopen(base_url, urllib.urlencode(params))
    print 'Done, request sent to server to produce build.'
  except IOError as e:
    raise ServerAccessError('%s is unaccessible. Reason: %s' % (base_url, e))
  if not connection:
    raise ServerAccessError('%s is unaccessible.' % base_url)

  response = connection.read()
  print 'Received response from server: %s' % response
  if response != 'OK':
    raise ServerAccessError('Response was not "OK".')
  return True


def _IsBuildRunning(build_data):
  """Checks whether the build is in progress on buildbot.

  Presence of currentStep element in build JSON indicates build is in progress.

  Args:
    build_data: A dictionary with build data, loaded from buildbot JSON API.

  Returns:
    True if build is in progress, otherwise False.
  """
  current_step = build_data.get('currentStep')
  if (current_step and current_step.get('isStarted') and
      current_step.get('results') is None):
    return True
  return False


def _IsBuildFailed(build_data):
  """Checks whether the build failed on buildbot.

  Sometime build status is marked as failed even though compile and packaging
  steps are successful. This may happen due to some intermediate steps of less
  importance such as gclient revert, generate_telemetry_profile are failed.
  Therefore we do an addition check to confirm if build was successful by
  calling _IsBuildSuccessful.

  Args:
    build_data: A dictionary with build data, loaded from buildbot JSON API.

  Returns:
    True if revision is failed build, otherwise False.
  """
  if (build_data.get('results') in FAILED and
      not _IsBuildSuccessful(build_data)):
    return True
  return False


def _IsBuildSuccessful(build_data):
  """Checks whether the build succeeded on buildbot.

  We treat build as successful if the package_build step is completed without
  any error i.e., when results attribute of the this step has value 0 or 1
  in its first element.

  Args:
    build_data: A dictionary with build data, loaded from buildbot JSON API.

  Returns:
    True if revision is successfully build, otherwise False.
  """
  if build_data.get('steps'):
    for item in build_data.get('steps'):
      # The 'results' attribute of each step consists of two elements,
      # results[0]: This represents the status of build step.
      # See: http://docs.buildbot.net/current/developer/results.html
      # results[1]: List of items, contains text if step fails, otherwise empty.
      if (item.get('name') == 'package_build' and
          item.get('isFinished') and
          item.get('results')[0] in OK):
        return True
  return False


def _FetchBuilderData(builder_url):
  """Fetches JSON data for the all the builds from the tryserver.

  Args:
    builder_url: A tryserver URL to fetch builds information.

  Returns:
    A dictionary with information of all build on the tryserver.
  """
  data = None
  try:
    url = urllib2.urlopen(builder_url)
  except urllib2.URLError as e:
    print ('urllib2.urlopen error %s, waterfall status page down.[%s]' % (
        builder_url, str(e)))
    return None
  if url is not None:
    try:
      data = url.read()
    except IOError as e:
      print 'urllib2 file object read error %s, [%s].' % (builder_url, str(e))
  return data


def _GetBuildData(buildbot_url):
  """Gets build information for the given build id from the tryserver.

  Args:
    buildbot_url: A tryserver URL to fetch build information.

  Returns:
    A dictionary with build information if build exists, otherwise None.
  """
  builds_json = _FetchBuilderData(buildbot_url)
  if builds_json:
    return json.loads(builds_json)
  return None


def _GetBuildBotUrl(builder_host, builder_port):
  """Gets build bot URL for fetching build info.

  Bisect builder bots are hosted on tryserver.chromium.perf, though we cannot
  access this tryserver using host and port number directly, so we use another
  tryserver URL for the perf tryserver.

  Args:
    builder_host: Hostname of the server where the builder is hosted.
    builder_port: Port number of ther server where the builder is hosted.

  Returns:
    URL of the buildbot as a string.
  """
  if (builder_host == PERF_BISECT_BUILDER_HOST and
      builder_port == PERF_BISECT_BUILDER_PORT):
    return PERF_TRY_SERVER_URL
  else:
    return 'http://%s:%s' % (builder_host, builder_port)


def GetBuildStatus(build_num, bot_name, builder_host, builder_port):
  """Gets build status from the buildbot status page for a given build number.

  Args:
    build_num: A build number on tryserver to determine its status.
    bot_name: Name of the bot where the build information is scanned.
    builder_host: Hostname of the server where the builder is hosted.
    builder_port: Port number of the server where the builder is hosted.

  Returns:
    A pair which consists of build status (SUCCESS, FAILED or PENDING) and a
    link to build status page on the waterfall.
  """
  results_url = None
  if build_num:
    # Get the URL for requesting JSON data with status information.
    server_url = _GetBuildBotUrl(builder_host, builder_port)
    buildbot_url = BUILDER_JSON_URL % {
        'server_url': server_url,
        'bot_name': bot_name,
        'build_num': build_num,
    }
    build_data = _GetBuildData(buildbot_url)
    if build_data:
      # Link to build on the buildbot showing status of build steps.
      results_url = BUILDER_HTML_URL % {
          'server_url': server_url,
          'bot_name': bot_name,
          'build_num': build_num,
      }
      if _IsBuildFailed(build_data):
        return (FAILED, results_url)

      elif _IsBuildSuccessful(build_data):
        return (OK, results_url)
  return (PENDING, results_url)


def GetBuildNumFromBuilder(build_reason, bot_name, builder_host, builder_port):
  """Gets build number on build status page for a given 'build reason'.

  This function parses the JSON data from buildbot page and collects basic
  information about the all the builds, and then uniquely identifies the build
  based on the 'reason' attribute in builds's JSON data.

  The 'reason' attribute set is when a build request is posted, and it is used
  to identify the build on status page.

  Args:
    build_reason: A unique build name set to build on tryserver.
    bot_name: Name of the bot where the build information is scanned.
    builder_host: Hostname of the server where the builder is hosted.
    builder_port: Port number of ther server where the builder is hosted.

  Returns:
    A build number as a string if found, otherwise None.
  """
  # Gets the buildbot url for the given host and port.
  server_url = _GetBuildBotUrl(builder_host, builder_port)
  buildbot_url = BUILDER_JSON_URL % {
      'server_url': server_url,
      'bot_name': bot_name,
      'build_num': '_all',
  }
  builds_json = _FetchBuilderData(buildbot_url)
  if builds_json:
    builds_data = json.loads(builds_json)
    for current_build in builds_data:
      if builds_data[current_build].get('reason') == build_reason:
        return builds_data[current_build].get('number')
  return None


def _GetRequestParams(options):
  """Extracts request parameters which will be passed to PostTryJob.

  Args:
    options: The options object parsed from the command line.

  Returns:
    A dictionary with parameters to pass to PostTryJob.
  """
  params = {
      'user': options.user,
      'name': options.name,
  }
  # Add other parameters if they're available in the options object.
  for key in ['email', 'revision', 'root', 'bot', 'patch']:
    option = getattr(options, key)
    if option:
      params[key] = option
  return params


def _GenParser():
  """Returns a parser for getting command line arguments."""
  usage = ('%prog [options]\n'
           'Post a build request to the try server for the given revision.')
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('-H', '--host',
                    help='Host address of the try server (required).')
  parser.add_option('-P', '--port', type='int',
                    help='HTTP port of the try server (required).')
  parser.add_option('-u', '--user', default=getpass.getuser(),
                    dest='user',
                    help='Owner user name [default: %default]')
  parser.add_option('-e', '--email',
                    default=os.environ.get('TRYBOT_RESULTS_EMAIL_ADDRESS',
                                           os.environ.get('EMAIL_ADDRESS')),
                    help=('Email address where to send the results. Use either '
                          'the TRYBOT_RESULTS_EMAIL_ADDRESS environment '
                          'variable or EMAIL_ADDRESS to set the email address '
                          'the try bots report results to [default: %default]'))
  parser.add_option('-n', '--name',
                    default='try_job_http',
                    help='Descriptive name of the try job')
  parser.add_option('-b', '--bot',
                    help=('Only one builder per run may be specified; to post '
                          'jobs on on multiple builders, run script for each '
                          'builder separately.'))
  parser.add_option('-r', '--revision',
                    help=('Revision to use for the try job. The revision may '
                          'be determined by the try server; see its waterfall '
                          'for more info.'))
  parser.add_option('--root',
                    help=('Root to use for the patch; base subdirectory for '
                          'patch created in a subdirectory.'))
  parser.add_option('--patch',
                    help='Patch information.')
  return parser


def Main(_):
  """Posts a try job based on command line parameters."""
  parser = _GenParser()
  options, _ = parser.parse_args()
  if not options.host or not options.port:
    parser.print_help()
    return 1
  params = _GetRequestParams(options)
  PostTryJob(options.host, options.port, params)


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
