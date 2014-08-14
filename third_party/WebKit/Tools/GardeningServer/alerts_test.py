# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import alerts
import json
import random
import string
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

    def test_large_number_of_alerts(self):
        # This generates ~2.5MB of JSON that compresses to ~750K. Real
        # data compresses about 6x better.
        random.seed(0xf00f00)
        put_alerts = self.generate_fake_alerts(4000)

        params = {'content': json.dumps(put_alerts)}
        self.testapp.post('/alerts', params)

        res = self.testapp.get('/alerts')
        got_alerts = json.loads(res.body)
        self.assertEquals(got_alerts['alerts'], put_alerts['alerts'])

    def generate_fake_alerts(self, n):
        return {'alerts': [self.generate_fake_alert() for _ in range(n)]}

    def generate_fake_alert(self):
        # fake labels
        labels = [['', 'last_', 'latest_', 'failing_', 'passing_'],
                  ['build', 'builder', 'revision'],
                  ['', 's', '_url', '_reason', '_name']]

        def label():
            return string.join(map(random.choice, labels), '')

        # fake values
        def time():
            return random.randint(1407976107614, 1408076107614) / 101.0

        def build():
            return random.randint(2737, 2894)

        def revision():
            return random.randint(288849, 289415)

        tests = [['Activity', 'Async', 'Browser', 'Content', 'Input'],
                 ['Manager', 'Card', 'Sandbox', 'Container'],
                 ['Test.'],
                 ['', 'Basic', 'Empty', 'More'],
                 ['Mouse', 'App', 'Selection', 'Network', 'Grab'],
                 ['Input', 'Click', 'Failure', 'Capture']]

        def test():
            return string.join(map(random.choice, tests), '')

        def literal_array():
            generator = random.choice([time, build, revision])
            return [generator() for _ in range(random.randint(0, 10))]

        def literal_map():
            generators = [build, revision, test, literal_array]
            obj = {}
            for _ in range(random.randint(3, 9)):
                obj[label()] = random.choice(generators)()
            return obj

        def value():
            generators = [time, build, revision, test, literal_array,
                          literal_map]
            return random.choice(generators)()

        alert = {}
        for _ in range(random.randint(6, 9)):
            alert[label()] = value()
        return alert
