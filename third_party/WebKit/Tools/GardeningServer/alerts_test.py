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
from google.appengine.ext import ndb
from google.appengine.ext import testbed


class AlertsTest(unittest.TestCase):
    def setUp(self):
        self.testbed = testbed.Testbed()
        self.testbed.activate()
        self.testbed.init_user_stub()
        self.testbed.init_memcache_stub()
        self.testbed.init_datastore_v3_stub()
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

    def test_alerts_jsons_are_stored_in_history(self):
        test_alerts1 = {'alerts': ['hello', 'world', '1']}
        test_alerts2 = {'alerts': ['hello', 'world', '2']}
        self.testapp.post('/alerts', {'content': json.dumps(test_alerts1)})
        self.testapp.post('/alerts', {'content': json.dumps(test_alerts2)})
        alerts_query = alerts.AlertsJSON.query().order(alerts.AlertsJSON.date)
        stored_alerts = alerts_query.fetch(limit=3)
        self.assertEqual(2, len(stored_alerts))
        self.assertEqual(stored_alerts[0].type, 'alerts')
        self.assertEqual(stored_alerts[1].type, 'alerts')
        stored_alerts1 = json.loads(stored_alerts[0].json)
        stored_alerts2 = json.loads(stored_alerts[1].json)
        self.assertEqual(test_alerts1['alerts'], stored_alerts1['alerts'])
        self.assertEqual(test_alerts2['alerts'], stored_alerts2['alerts'])
        self.assertTrue('date' in stored_alerts1)
        self.assertTrue('date' in stored_alerts2)
        self.assertEqual(type(stored_alerts1['date']), int)
        self.assertEqual(type(stored_alerts2['date']), int)

    def test_repeating_alerts_are_not_stored_to_history(self):
        test_alerts = {'alerts': ['hello', 'world']}
        self.testapp.post('/alerts', {'content': json.dumps(test_alerts)})
        test_alerts['last_builder_info'] = {'some': 'info'}
        self.testapp.post('/alerts', {'content': json.dumps(test_alerts)})
        stored_alerts = alerts.AlertsJSON.query().fetch(limit=2)
        self.assertEqual(1, len(stored_alerts))

    def test_alerts_jsons_are_retrieved_from_history(self):
        test_alert = {'alerts': ['hello', 'world', '1']}
        alerts.AlertsJSON(json=json.dumps(test_alert), type='alerts').put()
        response = self.testapp.get('/alerts-history')
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.content_type, 'application/json')
        parsed_json = json.loads(response.normal_body)
        self.assertEqual(len(parsed_json['history']), 1)

        entry_id = parsed_json['history'][0]
        response = self.testapp.get('/alerts-history/%s' % entry_id)
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.content_type, 'application/json')
        parsed_json = json.loads(response.normal_body)
        self.assertEqual(parsed_json['alerts'], test_alert['alerts'])

    def test_provides_login_url(self):
        response = self.testapp.get('/alerts-history')
        self.assertIn('login-url', response)

    def test_invalid_keys_return_400(self):
        response = self.testapp.get('/alerts-history/kjhg$%T',
                                    expect_errors=True)
        self.assertEqual(response.status_int, 400)
        self.assertEqual(response.content_type, 'application/json')

    def test_non_existing_keys_return_404(self):
        response = self.testapp.get('/alerts-history/5348024557502464',
                                    expect_errors=True)
        self.assertEqual(response.status_int, 404)
        self.assertEqual(response.content_type, 'application/json')

    def test_internal_alerts_can_only_retrieved_by_internal_users(self):
        test_alert = {'alerts': ['hello', 'world', '1']}
        internal_alert = alerts.AlertsJSON(json=json.dumps(test_alert),
                                           type='internal-alerts')
        internal_alert_key = internal_alert.put().integer_id()

        # No signed-in user.
        response = self.testapp.get('/alerts-history/%s' % internal_alert_key,
                                    expect_errors=True)
        self.assertEqual(response.status_int, 404)
        self.assertEqual(response.content_type, 'application/json')
        parsed_json = json.loads(response.normal_body)
        self.assertNotIn('alerts', parsed_json)

        # Non-internal user.
        self.testbed.setup_env(USER_EMAIL='test@example.com', USER_ID='1',
                               USER_IS_ADMIN='1', overwrite=True)
        response = self.testapp.get('/alerts-history/%s' % internal_alert_key,
                                    expect_errors=True)
        self.assertEqual(response.status_int, 404)
        self.assertEqual(response.content_type, 'application/json')
        parsed_json = json.loads(response.normal_body)
        self.assertNotIn('alerts', parsed_json)

    def test_lists_internal_alerts_to_internal_users_only(self):
        test_alert = {'alerts': ['hello', 'world', '1']}
        alerts.AlertsJSON(json=json.dumps(test_alert),
                          type='internal-alerts').put()

        # No signed-in user.
        response = self.testapp.get('/alerts-history')
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.content_type, 'application/json')
        response_json = json.loads(response.normal_body)
        self.assertEqual(len(response_json['history']), 0)

        # Non-internal user.
        self.testbed.setup_env(USER_EMAIL='test@example.com', USER_ID='1',
                               USER_IS_ADMIN='1', overwrite=True)
        response = self.testapp.get('/alerts-history')
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.content_type, 'application/json')
        response_json = json.loads(response.normal_body)
        self.assertEqual(len(response_json['history']), 0)

        # Internal user.
        self.testbed.setup_env(USER_EMAIL='test@google.com', USER_ID='2',
                               USER_IS_ADMIN='1', overwrite=True)
        response = self.testapp.get('/alerts-history')
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.content_type, 'application/json')
        response_json = json.loads(response.normal_body)
        self.assertEqual(len(response_json['history']), 1)

    def test_returned_alerts_from_history_are_paged(self):
        for i in range(20):
            test_alert = {'alerts': ['hello', 'world', i]}
            alerts.AlertsJSON(json=json.dumps(test_alert), type='alerts').put()

        response = self.testapp.get('/alerts-history?limit=15')
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.content_type, 'application/json')
        response_json = json.loads(response.normal_body)
        self.assertEqual(len(response_json['history']), 15)
        self.assertEqual(response_json['has_more'], True)

        url = '/alerts-history?limit=15&cursor=%s' % response_json['cursor']
        response = self.testapp.get(url)
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.content_type, 'application/json')
        response_json = json.loads(response.normal_body)
        self.assertEqual(len(response_json['history']), 5)
        self.assertEqual(response_json['has_more'], False)

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
