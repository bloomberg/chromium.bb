# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import datetime
import json
import logging
import re
import sys
import urllib2
import webapp2

from google.appengine.api import memcache

from handlers import master_config

# Buildbot steps that have test in the name, but don't run tests.
NON_TEST_STEP_NAMES = [
    'archive',
    'Run tests',
    'find isolated tests',
    'read test spec',
    'Download latest chromedriver',
    'compile tests',
    'create_coverage_',
    'update test result log',
    'memory test:',
    'install_',
]

# Buildbot steps that run tests but don't upload results to the flakiness
# dashboard server.
# FIXME: These should be fixed to upload and then removed from this list.
TEST_STEPS_THAT_DO_NOT_UPLOAD_YET = [
    'java_tests(chrome',
    'python_tests(chrome',
    'run_all_tests.py',
    'test_report',
    'test CronetSample',
    'test_mini_installer',
    'webkit_python_tests',
    'webkit_unit_tests',
]

BUILDS_URL_TEMPLATE = ('http://chrome-build-extract.appspot.com/get_builds?'
  'builder=%s&master=%s&num_builds=1')
MASTER_URL_TEMPLATE = 'http://chrome-build-extract.appspot.com/get_master/%s'


class FetchBuildersException(Exception):
  pass


def fetch_json(url):
  logging.debug('Fetching %s' % url)
  fetched_json = {}
  try:
    resp = urllib2.urlopen(url)
  except: # FIXME: This should be specific! # pylint: disable=W0702
    exc_info = sys.exc_info()
    logging.warning('Error while fetching %s: %s', url, exc_info[1])
    return fetched_json

  try:
    fetched_json = json.load(resp)
  except: # FIXME: This should be specific! # pylint: disable=W0702
    exc_info = sys.exc_info()
    logging.warning(
        'Unable to parse JSON response from %s: %s', url, exc_info[1])

  return fetched_json


def dump_json(data):
  return json.dumps(data, separators=(',', ':'), sort_keys=True)


def fetch_buildbot_data(masters=None):
  start_time = datetime.datetime.now()
  all_masters_data = []
  if masters:
    masters = [master_config.getMaster(m) for m in masters]
  else:
    masters = master_config.getAllMasters()

  for master_data in masters:
    all_masters_data.append(master_data)
    url_name = master_data['url_name']
    master_url = MASTER_URL_TEMPLATE % url_name
    builders = fetch_json(master_url)
    if not builders:
      msg = 'Aborting fetch. Could not fetch builders from %s' % master_url
      logging.warning(msg)
      raise FetchBuildersException(msg)

    tests_object = master_data.setdefault('tests', {})

    for builder in builders['builders'].keys():
      build = fetch_json(BUILDS_URL_TEMPLATE %
                         (urllib2.quote(builder), url_name))
      if not build:
        logging.info('Skipping builder %s on master %s due to empty data.',
            builder, url_name)
        continue

      if not build['builds']:
        logging.info(
            'Skipping builder %s on master %s due to empty builds list.',
            builder, url_name)
        continue

      for step in build['builds'][0]['steps']:
        step_name = step['name']

        if not 'test' in step_name:
          continue

        if any(name in step_name for name in NON_TEST_STEP_NAMES):
          continue

        if re.search('_only|_ignore|_perf$', step_name):
          continue

        if step_name == 'webkit_tests':
          step_name = 'layout-tests'

        tests_object.setdefault(step_name, {'builders': []})
        tests_object[step_name]['builders'].append(builder)

    for builders in tests_object.values():
      builders['builders'].sort()

  output_data = {'masters': all_masters_data,
                 'no_upload_test_types': TEST_STEPS_THAT_DO_NOT_UPLOAD_YET}

  delta = datetime.datetime.now() - start_time

  logging.info('Fetched buildbot data in %s seconds.', delta.seconds)

  return dump_json(output_data)


class UpdateBuilders(webapp2.RequestHandler):  # pylint: disable=W0232

  """Fetch and update the cached buildbot data."""

  def get(self):
    try:
      buildbot_data = fetch_buildbot_data()
      memcache.set('buildbot_data', buildbot_data)
      self.response.set_status(200)
      self.response.out.write("ok")
    except FetchBuildersException, ex:
      logging.error('Not updating builders because fetch failed: %s', str(ex))
      self.response.set_status(500)
      self.response.out.write(ex.message)


class GetBuilders(webapp2.RequestHandler):  # pylint: disable=W0232

  """Return a list of masters mapped to their respective builders,
  possibly using cached data."""

  def get(self):
    buildbot_data = memcache.get('buildbot_data')

    if not buildbot_data:
      logging.warning('No buildbot data in memcache. If this message repeats, '
          'something is probably wrong with memcache.')
      try:
        buildbot_data = fetch_buildbot_data()
        memcache.set('buildbot_data', buildbot_data)
      except FetchBuildersException, ex:
        logging.error('Builders fetch failed: %s', str(ex))
        self.response.set_status(500)
        self.response.out.write(ex.message)
        return

    callback = self.request.get('callback')
    if callback:
      buildbot_data = callback + '(' + buildbot_data + ');'

    self.response.out.write(buildbot_data)
