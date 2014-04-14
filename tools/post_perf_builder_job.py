# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Post a try job to the Try server to produce build. It communicates
to server by directly connecting via HTTP.
"""

import getpass
import optparse
import os
import sys
import urllib
import urllib2


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
                    help='Email address where to send the results. Use either '
                         'the TRYBOT_RESULTS_EMAIL_ADDRESS environment '
                         'variable or EMAIL_ADDRESS to set the email address '
                         'the try bots report results to [default: %default]')
  parser.add_option('-n', '--name',
                    default= 'try_job_http',
                    help='Descriptive name of the try job')
  parser.add_option('-b', '--bot',
                    help=('IMPORTANT: specify ONE builder per run is supported.'
                          'Run script for each builders separately.'))
  parser.add_option('-r', '--revision',
                    help='Revision to use for the try job; default: the '
                         'revision will be determined by the try server; see '
                         'its waterfall for more info')
  parser.add_option('--root',
                   help='Root to use for the patch; base subdirectory for '
                        'patch created in a subdirectory')
  parser.add_option('--patch',
                   help='Patch information.')
  return parser


def Main(argv):
  parser = _GenParser()
  options, args = parser.parse_args()
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
