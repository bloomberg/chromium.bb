# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import alerts
import json
import unittest
import webtest

from google.appengine.api import memcache
from google.appengine.ext import testbed


class AlertsTest(unittest.TestCase):
    def setUp(self):
        self.testbed = testbed.Testbed()
        self.testbed.activate()
        self.testbed.init_memcache_stub()
        self.testapp = webtest.TestApp(alerts.app)

    def tearDown(self):
        self.testbed.deactivate()

    def check_json_headers(self, res):
        self.assertEqual(res.content_type, 'application/json')
        # This is necessary for cross-site tools to retrieve alerts
        self.assertEqual(res.headers['access-control-allow-origin'], '*')

    def test_get_no_data_cached(self):
        res = self.testapp.get('/alerts')
        self.check_json_headers(res)
        self.assertEqual(res.body, '')

    def test_happy_path(self):
        # Set it.
        params = {'content': '{"alerts": ["hello", "world"]}'}
        self.testapp.post('/alerts', params)

        # Get it.
        res = self.testapp.get('/alerts')
        self.check_json_headers(res)
        alerts = json.loads(res.body)

        # The server should have stuck a 'date' on there.
        self.assertTrue('date' in alerts)
        self.assertEqual(type(alerts['date']), int)

        self.assertEqual(alerts['alerts'], ['hello', 'world'])

    def test_post_invalid_data_not_reflected(self):
        params = {'content': '[{"this is not valid JSON'}
        self.testapp.post('/alerts', params, status=400)
        res = self.testapp.get('/alerts')
        self.assertEqual(res.body, '')

    def test_post_invalid_data_does_not_overwrite_valid_data(self):
        # Populate the cache with something valid
        params = {'content': '{"alerts": "everything is OK"}'}
        self.testapp.post('/alerts', params)
        self.testapp.post('/alerts', {'content': 'woozlwuzl'}, status=400)
        res = self.testapp.get('/alerts')
        self.check_json_headers(res)
        alerts = json.loads(res.body)
        self.assertEqual(alerts['alerts'], 'everything is OK')
