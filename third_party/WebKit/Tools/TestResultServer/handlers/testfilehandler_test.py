#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import json
import imp
import logging
import os
import sys
import unittest

assert 'GAE_SDK_ROOT' in os.environ, ('''
You must set the environment variable GAE_SDK_ROOT to point to the
root of your google_appengine SDK installation before running the tests.''')

os.environ.setdefault('APPENGINE_RUNTIME', 'python27')

try:
    wrapper_util = imp.load_module(
        'wrapper_util', *imp.find_module(
            'wrapper_util', [os.environ['GAE_SDK_ROOT']]))
except ImportError:
    print >> sys.stderr, 'GAE_SDK_ROOT=%s does not look like the root of an appengine SDK installation.' % os.environ['GAE_SDK_ROOT']
    raise

APP_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path = wrapper_util.Paths(os.environ['GAE_SDK_ROOT']).v1_extra_paths + sys.path
sys.path.append(APP_ROOT)

import webapp2

try:
    import webtest
except ImportError:
    print "ERROR: Could not load webtest python module.  You may need to 'sudo apt-get python-webtest'."
    raise

from google.appengine.ext import blobstore
from google.appengine.ext import db
from google.appengine.ext import testbed

from handlers import master_config
from handlers import testfilehandler
from model.jsonresults import *


class TestFileHandlerTest(unittest.TestCase):

    APP_MAIN_MODULE = imp.load_module('app_main', *imp.find_module('main', [APP_ROOT]))

    def setUp(self):
        app = webapp2.WSGIApplication(self.APP_MAIN_MODULE.routes)
        self.testapp = webtest.TestApp(app)
        self.tb = testbed.Testbed()
        self.tb.activate()
        self.tb.init_datastore_v3_stub()
        self.tb.init_blobstore_stub()

    def tearDown(self):
        self.tb.deactivate()

    def test_basic_upload(self):
        master = master_config.getMaster('chromium.chromiumos')
        builder = 'test-builder'
        test_type = 'test-type'
        test_data = {
            'tests': {
                'Test1.testproc1': {
                    'expected': 'PASS',
                    'actual': 'FAIL',
                    'time': 1,
                }
            },
            'build_number': '123',
            'version': JSON_RESULTS_HIERARCHICAL_VERSION,
            'builder_name': builder,
            'blink_revision': '12345',
            'seconds_since_epoch': 1406123456,
            'num_failures_by_type': {
                'FAIL': 0,
                'SKIP': 0,
                'PASS': 1
            },
            'chromium_revision': '67890',
        }

        params = collections.OrderedDict([
            (testfilehandler.PARAM_BUILDER, builder),
            (testfilehandler.PARAM_MASTER, master['url_name']),
            (testfilehandler.PARAM_TEST_TYPE, test_type),
        ])
        upload_files = [('file', 'full_results.json', json.JSONEncoder().encode(test_data))]
        response = self.testapp.post('/testfile/upload', params=params, upload_files=upload_files)
        self.assertEqual(response.status_int, 200)

        # test aggregated results.json got generated
        params = collections.OrderedDict([
            (testfilehandler.PARAM_BUILDER, builder),
            (testfilehandler.PARAM_MASTER, master['url_name']),
            (testfilehandler.PARAM_TEST_TYPE, test_type),
            (testfilehandler.PARAM_NAME, 'results.json')
        ])
        response = self.testapp.get('/testfile', params=params)
        self.assertEqual(response.status_int, 200)
        response_json = json.loads(response.normal_body)
        self.assertEqual(response_json[builder]['tests']['Test1.testproc1'],
            {'results': [[1, 'Q']], 'times': [[1, 1]]})

        # test testlistjson=1
        params[testfilehandler.PARAM_TEST_LIST_JSON] = '1'

        response = self.testapp.get('/testfile', params=params)
        self.assertEqual(response.status_int, 200)
        response_json = json.loads(response.normal_body)
        self.assertEqual(response_json[builder]['tests']['Test1.testproc1'], {})

    def test_deprecated_master_name(self):
        """Verify that a file uploaded with a deprecated master name
        can be downloaded by the corresponding new-style master name.
        """
        master = master_config.getMaster('chromium.chromiumos')
        builder = 'test-builder'
        test_type = 'test-type'
        test_data = {
            'tests': {
                'Test1.testproc1': {
                    'expected': 'PASS',
                    'actual': 'PASS',
                    'time': 1,
                }
            },
            'build_number': '123',
            'version': JSON_RESULTS_HIERARCHICAL_VERSION,
            'builder_name': builder,
            'blink_revision': '12345',
            'seconds_since_epoch': 1406123456,
            'num_failures_by_type': {
                'FAIL': 0,
                'SKIP': 0,
                'PASS': 1
            },
            'chromium_revision': '67890',
        }

        # Upload file using deprecated master name.
        params = collections.OrderedDict([
            (testfilehandler.PARAM_BUILDER, builder),
            (testfilehandler.PARAM_MASTER, master['name']),
            (testfilehandler.PARAM_TEST_TYPE, test_type),
        ])
        upload_files = [('file', 'full_results.json', json.JSONEncoder().encode(test_data))]
        response = self.testapp.post('/testfile/upload', params=params, upload_files=upload_files)
        self.assertEqual(response.status_int, 200)

        # Download file using deprecated master name.
        params = collections.OrderedDict([
            (testfilehandler.PARAM_BUILDER, builder),
            (testfilehandler.PARAM_MASTER, master['name']),
            (testfilehandler.PARAM_TEST_TYPE, test_type),
            (testfilehandler.PARAM_BUILD_NUMBER, '123'),
            (testfilehandler.PARAM_NAME, 'full_results.json')
        ])
        response = self.testapp.get('/testfile', params=params)
        self.assertEqual(response.status_int, 200)
        response_json = json.loads(response.normal_body)
        self.assertEqual(response_json['chromium_revision'], '67890')

        # Download file using new-style name.
        params = collections.OrderedDict([
            (testfilehandler.PARAM_BUILDER, builder),
            (testfilehandler.PARAM_MASTER, master['url_name']),
            (testfilehandler.PARAM_TEST_TYPE, test_type),
            (testfilehandler.PARAM_BUILD_NUMBER, '123'),
            (testfilehandler.PARAM_NAME, 'full_results.json')
        ])
        response = self.testapp.get('/testfile', params=params)
        self.assertEqual(response.status_int, 200)
        response_json = json.loads(response.normal_body)
        self.assertEqual(response_json['chromium_revision'], '67890')

if __name__ == '__main__':
    logging.basicConfig(level=logging.ERROR)
    unittest.main()
