# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import unittest
import urllib2

from webkitpy.common.net.rietveld import filter_latest_jobs
from webkitpy.common.net.rietveld import get_latest_try_job_results
from webkitpy.common.net.rietveld import latest_try_jobs
from webkitpy.common.net.buildbot import Build
from webkitpy.common.net.web_mock import MockWeb
from webkitpy.common.system.outputcapture import OutputCapture


_log = logging.getLogger(__name__)


class RietveldTest(unittest.TestCase):

    def setUp(self):
        self.web = MockWeb(urls={
            'https://codereview.chromium.org/api/11112222': json.dumps({
                'patchsets': [1, 2, 3],
            }),
            'https://codereview.chromium.org/api/11112222/2': json.dumps({
                'try_job_results': [
                    {
                        'builder': 'foo-builder',
                        'buildnumber': 10,
                        'result': -1
                    },
                    {
                        'builder': 'bar-builder',
                        'buildnumber': 50,
                        'results': 0
                    },
                ],
            }),
            'https://codereview.chromium.org/api/11112222/3': json.dumps({
                'try_job_results': [
                    {
                        'builder': 'foo-builder',
                        'buildnumber': 20,
                        'result': 1
                    },
                    {
                        'builder': 'bar-builder',
                        'buildnumber': 60,
                        'result': 0
                    },
                ],
            }),
            'https://codereview.chromium.org/api/11113333': 'my non-JSON contents',
        })

    def test_latest_try_jobs(self):
        self.assertEqual(
            latest_try_jobs(11112222, ('bar-builder', 'other-builder'), self.web),
            [Build('bar-builder', 60)])

    def test_latest_try_jobs_http_error(self):
        def raise_error(_):
            raise urllib2.URLError('Some request error message')
        self.web.get_binary = raise_error
        oc = OutputCapture()
        try:
            oc.capture_output()
            self.assertEqual(latest_try_jobs(11112222, ('bar-builder',), self.web), [])
        finally:
            _, _, logs = oc.restore_output()
        self.assertEqual(logs, 'Request failed to URL: https://codereview.chromium.org/api/11112222\n')

    def test_latest_try_jobs_non_json_response(self):
        oc = OutputCapture()
        try:
            oc.capture_output()
            self.assertEqual(latest_try_jobs(11113333, ('bar-builder',), self.web), [])
        finally:
            _, _, logs = oc.restore_output()
        self.assertEqual(logs, 'Invalid JSON: my non-JSON contents\n')

    def test_latest_try_jobs_with_patchset(self):
        self.assertEqual(
            latest_try_jobs(11112222, ('bar-builder', 'other-builder'), self.web, patchset_number=2),
            [Build('bar-builder', 50)])

    def test_latest_try_jobs_no_relevant_builders(self):
        self.assertEqual(latest_try_jobs(11112222, ('foo', 'bar'), self.web), [])

    def test_get_latest_try_job_results(self):
        self.assertEqual(get_latest_try_job_results(11112222, self.web), {'foo-builder': 1, 'bar-builder': 0})

    def test_filter_latest_jobs_empty(self):
        self.assertEqual(filter_latest_jobs([]), [])

    def test_filter_latest_jobs_higher_build_first(self):
        self.assertEqual(
            filter_latest_jobs([Build('foo', 5), Build('foo', 3), Build('bar', 5)]),
            [Build('foo', 5), Build('bar', 5)])

    def test_filter_latest_jobs_higher_build_last(self):
        self.assertEqual(
            filter_latest_jobs([Build('foo', 3), Build('bar', 5), Build('foo', 5)]),
            [Build('bar', 5), Build('foo', 5)])

    def test_filter_latest_jobs_no_build_number(self):
        self.assertEqual(
            filter_latest_jobs([Build('foo', 3), Build('bar')]),
            [Build('foo', 3)])
