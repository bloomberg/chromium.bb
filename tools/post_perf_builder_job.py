# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Post a try job request via HTTP to the Tryserver to produce build."""

import getpass
import json
import optparse
import os
import sys
import urllib
import urllib2

# Link to get JSON data of builds
BUILDER_JSON_URL = ('%(server_url)s/json/builders/%(bot_name)s/builds/'
                    '%(build_num)s?as_text=1&filter=0')

# Link to display build steps
BUILDER_HTML_URL = ('%(server_url)s/builders/%(bot_name)s/builds/%(build_num)s')

# Tryserver buildbots status page
TRY_SERVER_URL = 'http://build.chromium.org/p/tryserver.chromium.perf'

# Hostname of the tryserver where perf bisect builders are hosted. This is used
# for posting build request to tryserver.
BISECT_BUILDER_HOST = 'master4.golo.chromium.org'
# 'try_job_port' on tryserver to post build request.
BISECT_BUILDER_PORT = 8341


# From buildbot.status.builder.
# See: http://docs.buildbot.net/current/developer/results.html
SUCCESS, WARNINGS, FAILURE, SKIPPED, EXCEPTION, RETRY, TRYPENDING = range(7)

# Status codes that can be returned by the GetBuildStatus method.
OK = (SUCCESS, WARNINGS)
# Indicates build failure.
FAILED = (FAILURE, EXCEPTION, SKIPPED)
# Inidcates build in progress or in pending queue.
PENDING = (RETRY, TRYPENDING)


class ServerAccessError(Exception):

  def __str__(self):
    return '%s\nSorry, cannot connect to server.' % self.args[0]


def PostTryJob(url_params):
  """Sends a build request to the server using the HTTP protocol.

  Args:
    url_params: A dictionary of query parameters to be sent in the request.
                In order to post build request to try server, this dictionary
                should contain information for following keys:
                'host': Hostname of the try server.
                'port': Port of the try server.
                'revision': SVN Revision to build.
                'bot': Name of builder bot which would be used.
  Returns:
    True if the request is posted successfully. Otherwise throws an exception.
  """
  # Parse url parameters to be sent to Try server.
  if not url_params.get('host'):
    raise ValueError('Hostname of server to connect is missing.')
  if not url_params.get('port'):
    raise ValueError('Port of server to connect is missing.')
  if not url_params.get('revision'):
    raise ValueError('Missing revision details. Please specify revision'
                     ' information.')
  if not url_params.get('bot'):
    raise ValueError('Missing bot details. Please specify bot information.')

  # Pop 'host' and 'port' to avoid passing them as query params.
  url = 'http://%s:%s/send_try_patch' % (url_params.pop('host'),
                                         url_params.pop('port'))

  print 'Sending by HTTP'
  query_params = '&'.join('%s=%s' % (k, v) for k, v in url_params.iteritems())
  print 'url: %s?%s' % (url, query_params)

  connection = None
  try:
    print 'Opening connection...'
    connection = urllib2.urlopen(url, urllib.urlencode(url_params))
    print 'Done, request sent to server to produce build.'
  except IOError, e:
    raise ServerAccessError('%s is unaccessible. Reason: %s' % (url, e))
  if not connection:
    raise ServerAccessError('%s is unaccessible.' % url)
  response = connection.read()
  print 'Received %s from server' % response
  if response != 'OK':
    raise ServerAccessError('%s is unaccessible. Got:\n%s' % (url, response))
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
  except urllib2.URLError, e:
    print ('urllib2.urlopen error %s, waterfall status page down.[%s]' % (
        builder_url, str(e)))
    return None
  if url is not None:
    try:
      data = url.read()
    except IOError, e:
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
  """Gets build bot URL based on the host and port of the builders.

  Note: All bisect builder bots are hosted on tryserver.chromium i.e.,
  on master4:8328, since we cannot access tryserver using host and port
  number directly, we use tryserver URL.

  Args:
    builder_host: Hostname of the server where the builder is hosted.
    builder_port: Port number of ther server where the builder is hosted.

  Returns:
    URL of the buildbot as a string.
  """
  if (builder_host == BISECT_BUILDER_HOST and
      builder_port == BISECT_BUILDER_PORT):
    return TRY_SERVER_URL
  else:
    return 'http://%s:%s' % (builder_host, builder_port)


def GetBuildStatus(build_num, bot_name, builder_host, builder_port):
  """Gets build status from the buildbot status page for a given build number.

  Args:
    build_num: A build number on tryserver to determine its status.
    bot_name: Name of the bot where the build information is scanned.
    builder_host: Hostname of the server where the builder is hosted.
    builder_port: Port number of ther server where the builder is hosted.

  Returns:
    A tuple consists of build status (SUCCESS, FAILED or PENDING) and a link
    to build status page on the waterfall.
  """
  results_url = None
  if build_num:
    # Gets the buildbot url for the given host and port.
    server_url = _GetBuildBotUrl(builder_host, builder_port)
    buildbot_url = BUILDER_JSON_URL % {'server_url': server_url,
                                       'bot_name': bot_name,
                                       'build_num': build_num
                                      }
    build_data = _GetBuildData(buildbot_url)
    if build_data:
      # Link to build on the buildbot showing status of build steps.
      results_url = BUILDER_HTML_URL % {'server_url': server_url,
                                        'bot_name': bot_name,
                                        'build_num': build_num
                                       }
      if _IsBuildFailed(build_data):
        return (FAILED, results_url)

      elif _IsBuildSuccessful(build_data):
        return (OK, results_url)
  return (PENDING, results_url)


def GetBuildNumFromBuilder(build_reason, bot_name, builder_host, builder_port):
  """Gets build number on build status page for a given build reason.

  It parses the JSON data from buildbot page and collect basic information
  about the all the builds and then this uniquely identifies the build based
  on the 'reason' attribute in builds's JSON data.
  The 'reason' attribute set while a build request is posted, and same is used
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
  buildbot_url = BUILDER_JSON_URL % {'server_url': server_url,
                                     'bot_name': bot_name,
                                     'build_num': '_all'
                                    }
  builds_json = _FetchBuilderData(buildbot_url)
  if builds_json:
    builds_data = json.loads(builds_json)
    for current_build in builds_data:
      if builds_data[current_build].get('reason') == build_reason:
        return builds_data[current_build].get('number')
  return None


def _GetQueryParams(options):
  """Parses common query parameters which will be passed to PostTryJob.

  Args:
    options: The options object parsed from the command line.

  Returns:
    A dictionary consists of query parameters.
  """
  values = {'host': options.host,
            'port': options.port,
            'user': options.user,
            'name': options.name
           }
  if options.email:
    values['email'] = options.email
  if options.revision:
    values['revision'] = options.revision
  if options.root:
    values['root'] = options.root
  if options.bot:
    values['bot'] = options.bot
  if options.patch:
    values['patch'] = options.patch
  return values


def _GenParser():
  """Parses the command line for posting build request."""
  usage = ('%prog [options]\n'
           'Post a build request to the try server for the given revision.\n')
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('-H', '--host',
                    help='Host address of the try server.')
  parser.add_option('-P', '--port', type='int',
                    help='HTTP port of the try server.')
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
                    help=('IMPORTANT: specify ONE builder per run is supported.'
                          'Run script for each builders separately.'))
  parser.add_option('-r', '--revision',
                    help=('Revision to use for the try job; default: the '
                          'revision will be determined by the try server; see '
                          'its waterfall for more info'))
  parser.add_option('--root',
                    help=('Root to use for the patch; base subdirectory for '
                          'patch created in a subdirectory'))
  parser.add_option('--patch',
                    help='Patch information.')
  return parser


def Main(argv):
  parser = _GenParser()
  options, _ = parser.parse_args()
  if not options.host:
    raise ServerAccessError('Please use the --host option to specify the try '
                            'server host to connect to.')
  if not options.port:
    raise ServerAccessError('Please use the --port option to specify the try '
                            'server port to connect to.')
  params = _GetQueryParams(options)
  PostTryJob(params)


if __name__ == '__main__':
  sys.exit(Main(sys.argv))

