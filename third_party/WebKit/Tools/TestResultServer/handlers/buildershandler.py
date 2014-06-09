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
import traceback
import urllib2
import webapp2

from google.appengine.api import memcache

MASTERS = [
    {'name': 'ChromiumWin', 'url': 'http://build.chromium.org/p/chromium.win', 'groups': ['@ToT Chromium']},
    {'name': 'ChromiumMac', 'url': 'http://build.chromium.org/p/chromium.mac', 'groups': ['@ToT Chromium']},
    {'name': 'ChromiumLinux', 'url': 'http://build.chromium.org/p/chromium.linux', 'groups': ['@ToT Chromium']},
    {'name': 'ChromiumChromiumOS', 'url': 'http://build.chromium.org/p/chromium.chromiumos', 'groups': ['@ToT ChromeOS']},
    {'name': 'ChromiumGPU', 'url': 'http://build.chromium.org/p/chromium.gpu', 'groups': ['@ToT Chromium']},
    {'name': 'ChromiumGPUFYI', 'url': 'http://build.chromium.org/p/chromium.gpu.fyi', 'groups': ['@ToT Chromium FYI']},
    {'name': 'ChromiumWebkit', 'url': 'http://build.chromium.org/p/chromium.webkit', 'groups': ['@ToT Chromium', '@ToT Blink']},
    {'name': 'ChromiumFYI', 'url': 'http://build.chromium.org/p/chromium.fyi', 'groups': ['@ToT Chromium FYI']},
    {'name': 'V8', 'url': 'http://build.chromium.org/p/client.v8', 'groups': ['@ToT V8']},
]

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

# Buildbot steps that run tests but don't upload results to the flakiness dashboard server.
# FIXME: These should be fixed to upload and then removed from this list.
TEST_STEPS_THAT_DO_NOT_UPLOAD_YET = [
    'java_tests(chrome',
    'python_tests(chrome',
    'run_all_tests.py',
    'test_report',
    'test CronetSample',
    'test_mini_installer',
    'telemetry_unittests',
    'webkit_python_tests',
    'webkit_unit_tests',
]

class FetchBuildersException(Exception): pass


def master_json_url(master_url):
    return master_url + '/json/builders'


def builder_json_url(master_url, builder):
    return master_json_url(master_url) + '/' + urllib2.quote(builder)


def cached_build_json_url(master_url, builder, build_number):
    return builder_json_url(master_url, builder) + '/builds/' + str(build_number)


def fetch_json(url):
    logging.debug('Fetching %s' % url)
    fetched_json = {}
    try:
        resp = urllib2.urlopen(url)
    except:
        exc_info = sys.exc_info()
        logging.warning('Error while fetching %s: %s', url, exc_info[1])
        return fetched_json

    try:
        fetched_json = json.load(resp)
    except:
        exc_info = sys.exc_info()
        logging.warning('Unable to parse JSON response from %s: %s', url, exc_info[1])

    return fetched_json


def get_latest_build(build_data):
    cached_builds = []
    if 'cachedBuilds' in build_data:
        cached_builds = build_data['cachedBuilds']

    current_builds = build_data['currentBuilds']

    latest_cached_builds = set(cached_builds) - set(current_builds)
    if len(latest_cached_builds) != 0:
        latest_cached_builds = sorted(list(latest_cached_builds))
        latest_build = latest_cached_builds[-1]
    elif len(current_builds) != 0:
        latest_build = current_builds[0]
    else:
        basedir = build_data['basedir'] if 'basedir' in build_data else 'current builder'
        logging.info('No cached or current builds for %s', basedir)
        return None

    return latest_build


def dump_json(data):
    return json.dumps(data, separators=(',', ':'), sort_keys=True)


def fetch_buildbot_data(masters, force_update=False):
    if force_update:
        logging.info('Starting a forced buildbot update. Failure to fetch a master\'s data will not abort the fetch.')

    start_time = datetime.datetime.now()
    master_data = masters[:]
    for master in master_data:
        master_url = master['url']
        tests_object = master.setdefault('tests', {})
        master['tests'] = tests_object

        builders = fetch_json(master_json_url(master_url))
        if not builders:
            msg = 'Could not fetch builders from master "%s": %s.' % (master['name'], master_url)
            logging.warning(msg)
            if force_update:
                continue
            else:
                logging.warning('Aborting fetch.')
                raise FetchBuildersException(msg)

        for builder in builders:
            build_data = fetch_json(builder_json_url(master_url, builder))

            latest_build = get_latest_build(build_data)
            if not latest_build:
                logging.info('Skipping builder %s because it lacked cached or current builds.', builder)
                continue

            build = fetch_json(cached_build_json_url(master_url, builder, latest_build))
            if not build:
                logging.info('Skipping build %s on builder %s due to empty data', latest_build, builder)
            for step in build['steps']:
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

    output_data = {'masters': master_data, 'no_upload_test_types': TEST_STEPS_THAT_DO_NOT_UPLOAD_YET}

    delta = datetime.datetime.now() - start_time

    logging.info('Fetched buildbot data in %s seconds.', delta.seconds)

    return dump_json(output_data)


class UpdateBuilders(webapp2.RequestHandler):
    """Fetch and update the cached buildbot data."""
    def get(self):
        force_update = True if self.request.get('force') else False
        try:
            buildbot_data = fetch_buildbot_data(MASTERS, force_update)
            memcache.set('buildbot_data', buildbot_data)
            self.response.set_status(200)
            self.response.out.write("ok")
        except FetchBuildersException, ex:
            logging.error('Not updating builders because fetch failed: %s', str(ex))
            self.response.set_status(500)
            self.response.out.write(ex.message)



class GetBuilders(webapp2.RequestHandler):
    """Return a list of masters mapped to their respective builders, possibly using cached data."""
    def get(self):
        callback = self.request.get('callback')

        buildbot_data = memcache.get('buildbot_data')

        if not buildbot_data:
            logging.warning('No buildbot data in memcache. If this message repeats, something is probably wrong with memcache.')

            # Since we have no cached buildbot data, we would rather have missing masters than no data at all.
            buildbot_data = fetch_buildbot_data(MASTERS, True)
            try:
                memcache.set('buildbot_data', buildbot_data)
            except ValueError, err:
                logging.error(str(err))

        if callback:
            buildbot_data = callback + '(' + buildbot_data + ');'

        self.response.out.write(buildbot_data)
