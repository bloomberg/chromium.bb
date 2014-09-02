# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import internal_alerts
import json
import random
import string
import unittest
import webtest

from google.appengine.api import memcache
from google.appengine.ext import testbed


class InternalAlertsTest(unittest.TestCase):
    def setUp(self):
        self.testbed = testbed.Testbed()
        self.testbed.activate()
        self.testbed.init_user_stub()
        self.testbed.init_memcache_stub()
        self.testapp = webtest.TestApp(internal_alerts.app)

    def tearDown(self):
        self.testbed.deactivate()

    def user_helper(self, email, uid):
        self.testbed.setup_env(
            USER_EMAIL=email,
            USER_ID=uid,
            USER_IS_ADMIN='0',
            overwrite=True)

    def check_json_headers(self, res):
        self.user_helper('tester@google.com', '123')
        self.assertEqual(res.content_type, 'application/json')
        # This is necessary for cross-site tools to retrieve internal alerts.
        self.assertEqual(res.headers['access-control-allow-origin'], '*')

    def test_get_no_data_cached(self):
        self.user_helper('tester@google.com', '123')
        res = self.testapp.get('/internal-alerts')
        self.check_json_headers(res)
        self.assertEqual(res.body, '')

    def test_happy_path(self):
        self.user_helper('tester@google.com', '123')
        # Set it.
        params = {'content': '{"alerts": ["hello", "world"]}'}
        self.testapp.post('/internal-alerts', params)

        # Get it.
        res = self.testapp.get('/internal-alerts')
        self.check_json_headers(res)
        internal_alerts = json.loads(res.body)

        # The server should have stuck a 'date' on there.
        self.assertTrue('date' in internal_alerts)
        self.assertEqual(type(internal_alerts['date']), int)

        self.assertEqual(internal_alerts['alerts'], ['hello', 'world'])

    def test_post_invalid_data_not_reflected(self):
        self.user_helper('tester@google.com', '123')
        params = {'content': '[{"this is not valid JSON'}
        self.testapp.post('/internal-alerts', params, status=400)
        res = self.testapp.get('/internal-alerts')
        self.assertEqual(res.body, '')

    def test_post_invalid_data_does_not_overwrite_valid_data(self):
        self.user_helper('tester@google.com', '123')
        # Populate the cache with something valid
        params = {'content': '{"alerts": "everything is OK"}'}
        self.testapp.post('/internal-alerts', params)
        self.testapp.post('/internal-alerts', {'content': 'woozlwuzl'},
                          status=400)
        res = self.testapp.get('/internal-alerts')
        self.check_json_headers(res)
        internal_alerts = json.loads(res.body)
        self.assertEqual(internal_alerts['alerts'], 'everything is OK')

    def test_large_number_of_internal_alerts(self):
        self.user_helper('tester@google.com', '123')
        # This generates ~2.5MB of JSON that compresses to ~750K. Real
        # data compresses about 6x better.
        random.seed(0xf00f00)
        put_internal_alerts = self.generate_fake_internal_alerts(4000)

        params = {'content': json.dumps(put_internal_alerts)}
        self.testapp.post('/internal-alerts', params)

        res = self.testapp.get('/internal-alerts')
        got_internal_alerts = json.loads(res.body)
        self.assertEquals(got_internal_alerts['alerts'],
                          put_internal_alerts['alerts'])

    def test_no_user(self):
        # Get it.
        res = self.testapp.get('/internal-alerts')
        self.check_json_headers(res)
        internal_alerts = json.loads(res.body)

        # The server should have stuck a 'date' on there.
        self.assertTrue('date' in internal_alerts)
        self.assertEqual(type(internal_alerts['date']), int)

        self.assertTrue('redirect-url' in internal_alerts)
        self.assertEqual(type(internal_alerts['redirect-url']), unicode)

    def test_invalid_user(self):
        self.user_helper('tester@chromium.org', '123')
        # Get it.
        res = self.testapp.get('/internal-alerts', status=403)

    def generate_fake_internal_alerts(self, n):
        self.user_helper('tester@google.com', '123')
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
