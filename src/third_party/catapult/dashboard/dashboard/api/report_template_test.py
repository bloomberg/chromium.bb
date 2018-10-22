# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import json
import unittest

from dashboard.api import api_auth
from dashboard.api import report_template as api_report_template
from dashboard.common import testing_common
from dashboard.models import report_template

@report_template.Static(
    internal_only=False,
    template_id=464092444,
    name='Test:ExternalTemplate',
    modified=datetime.datetime.now())
def _External(unused_revisions):
  return 'external'


@report_template.Static(
    internal_only=True,
    template_id=130723169,
    name='Test:InternalTemplate',
    modified=datetime.datetime.now())
def _Internal(unused_revisions):
  return 'internal'


class ReportTemplateTest(testing_common.TestCase):

  def setUp(self):
    super(ReportTemplateTest, self).setUp()
    self.SetUpApp([
        ('/api/report/template', api_report_template.ReportTemplateHandler),
    ])
    self.SetCurrentClientIdOAuth(api_auth.OAUTH_CLIENT_ID_WHITELIST[0])

  def _Post(self, **params):
    return json.loads(self.Post('/api/report/template', params).body)

  def testInvalid(self):
    self.Post('/api/report/template', dict(
        template=json.dumps({'rows': []})), status=400)
    self.Post('/api/report/template', dict(
        name='name', template=json.dumps({'rows': []})), status=400)
    self.Post('/api/report/template', dict(
        owners='o', template=json.dumps({'rows': []})), status=400)

  def testInternal_PutTemplate(self):
    self.SetCurrentUserOAuth(testing_common.INTERNAL_USER)
    response = self._Post(
        owners=testing_common.INTERNAL_USER.email(),
        name='Test:New',
        template=json.dumps({'rows': []}))
    names = [d['name'] for d in response]
    self.assertIn('Test:ExternalTemplate', names)
    self.assertIn('Test:InternalTemplate', names)
    self.assertIn('Test:New', names)

    template = report_template.ReportTemplate.query(
        report_template.ReportTemplate.name == 'Test:New').get()
    self.assertEqual({'rows': []}, template.template)

  def testAnonymous_PutTemplate(self):
    self.SetCurrentUserOAuth(None)
    self.Post('/api/report/template', dict(
        template=json.dumps({'rows': []}), name='n', owners='o'), status=401)


if __name__ == '__main__':
  unittest.main()
