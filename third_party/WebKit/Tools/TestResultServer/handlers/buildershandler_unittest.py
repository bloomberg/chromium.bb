#!/usr/bin/env python
# Copyright (C) 2012 Google Inc. All rights reserved.
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

import buildershandler
import json
import logging
import pprint
import unittest


class BuildersHandlerTest(unittest.TestCase):
    def test_master_json_url(self):
        self.assertEqual(buildershandler.master_json_url('http://base'), 'http://base/json/builders')

    def test_builder_json_url(self):
        self.assertEqual(buildershandler.builder_json_url('http://base', 'dummybuilder'), 'http://base/json/builders/dummybuilder')

    def test_cached_build_json_url(self):
        self.assertEqual(buildershandler.cached_build_json_url('http://base', 'dummybuilder', 12345), 'http://base/json/builders/dummybuilder/builds/12345')
        self.assertEqual(buildershandler.cached_build_json_url('http://base', 'dummybuilder', '12345'), 'http://base/json/builders/dummybuilder/builds/12345')

    def test_get_latest_build(self):
        build_data = {'cachedBuilds': ['1', '2', '3'],
                      'currentBuilds': ['3'],
                      'basedir': 'fake'}
        latest_build = buildershandler.get_latest_build(build_data)

        self.assertEqual(latest_build, '2')

        build_data = {'cachedBuilds': [],
                      'currentBuilds': ['1', '2', '3'],
                      'basedir': 'fake'}
        latest_build = buildershandler.get_latest_build(build_data)

        self.assertEqual(latest_build, '1')

        build_data = {'cachedBuilds': ['1', '2', '3'],
                      'currentBuilds': ['1', '2', '3'],
                      'basedir': 'fake'}
        latest_build = buildershandler.get_latest_build(build_data)

        self.assertEqual(latest_build, '1')

        build_data = {'cachedBuilds': [],
                      'currentBuilds': [],
                      'basedir': 'fake'}
        latest_build = buildershandler.get_latest_build(build_data)

        self.assertEqual(latest_build, None)

    def test_fetch_buildbot_data(self):
        try:
            fetched_urls = []

            def fake_fetch_json(url):
                fetched_urls.append(url)
                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders':
                    return {'WebKit Win': None, 'WebKit Linux': None, 'WebKit Mac': None, 'WebKit Empty': None}

                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Linux':
                    return {'cachedBuilds': [1, 2], 'currentBuilds': []}
                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Win':
                    return {'cachedBuilds': [1, 2], 'currentBuilds': []}
                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Mac':
                    return {'cachedBuilds': [1, 2], 'currentBuilds': []}
                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Empty':
                    return {'cachedBuilds': [], 'currentBuilds': []}

                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Linux/builds/2':
                    return {'steps': [{'name': 'webkit_tests'}, {'name': 'browser_tests'}, {'name': 'mini_installer_test'}, {'name': 'archive_test_results'}, {'name': 'compile'}]}
                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Win/builds/2':
                    return {'steps': [{'name': 'webkit_tests'}, {'name': 'mini_installer_test'}, {'name': 'archive_test_results'}, {'name': 'compile'}]}
                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Mac/builds/2':
                    return {'steps': [{'name': 'browser_tests'}, {'name': 'mini_installer_test'}, {'name': 'archive_test_results'}, {'name': 'compile'}]}

                logging.error('Cannot fetch fake url: %s' % url)

            old_fetch_json = buildershandler.fetch_json
            buildershandler.fetch_json = fake_fetch_json

            masters = [
                {'name': 'ChromiumWebkit', 'url': 'http://build.chromium.org/p/chromium.webkit'},
            ]

            buildbot_data = buildershandler.fetch_buildbot_data(masters)

            expected_fetched_urls = [
                'http://build.chromium.org/p/chromium.webkit/json/builders',
                'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Linux',
                'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Linux/builds/2',
                'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Mac',
                'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Mac/builds/2',
                'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Win',
                'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Win/builds/2',
                'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Empty',
            ]
            self.assertEqual(fetched_urls, expected_fetched_urls)

            expected_masters = {
                'masters': [{
                    'url': 'http://build.chromium.org/p/chromium.webkit',
                    'tests': {
                        'browser_tests': {'builders': ['WebKit Linux', 'WebKit Mac']},
                        'mini_installer_test': {'builders': ['WebKit Linux', 'WebKit Mac', 'WebKit Win']},
                        'layout-tests': {'builders': ['WebKit Linux', 'WebKit Win']}},
                    'name': 'ChromiumWebkit'}]}
            expected_json = buildershandler.dump_json(expected_masters)

            self.assertEqual(buildbot_data, expected_json)

        finally:
            buildershandler.fetch_json = old_fetch_json


if __name__ == '__main__':
    unittest.main()
