# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is expected to be used under another directory to use,
# so we disable checking import path of GAE tools from this directory.
# pylint: disable=F0401,E0611

import json
import unittest

from google.appengine.api import files
from google.appengine.ext import ndb
from google.appengine.ext import testbed
from google.appengine.ext.blobstore import BlobInfo

import services


class ServicesTest(unittest.TestCase):
  @staticmethod
  def CreateBlob(path):
    # Initialize blob dictionary to return.
    blob = {}

    # Read sample file.
    blob['json_str'] = open(path, 'r').read()

    # Create file in blobstore according to sample file.
    file_name = files.blobstore.create(mime_type='text/plain')
    with files.open(file_name, 'a') as f:
      f.write(blob['json_str'])
    files.finalize(file_name)

    # Get BlobInfo of sample file.
    blob['blob_info'] = BlobInfo.get(files.blobstore.get_blob_key(file_name))

    return blob

  def setUp(self):
    self.testbed = testbed.Testbed()
    self.testbed.activate()
    self.testbed.init_all_stubs()

    # Read sample file.
    self.correct_blob = ServicesTest.CreateBlob('testdata/sample.json')
    self.error_blob = ServicesTest.CreateBlob('testdata/error_sample.json')

  def tearDown(self):
    self.testbed.deactivate()

  def testProfiler(self):
    correct_blob = self.correct_blob
    # Call services function to create Profiler entity.
    run_id = services.CreateProfiler(correct_blob['blob_info'])

    # Test GetProfiler
    self.assertEqual(services.GetProfiler(run_id), correct_blob['json_str'])

    # Create Profiler entity with the same file again and check uniqueness.
    services.CreateProfiler(correct_blob['blob_info'])
    self.assertEqual(services.Profiler.query().count(), 1)

  def testTemplate(self):
    correct_blob = self.correct_blob
    # Call services function to create template entities.
    services.CreateTemplates(correct_blob['blob_info'])

    # Test templates being stored in database correctly.
    json_obj = json.loads(correct_blob['json_str'])
    for content in json_obj['templates'].values():
      template_entity = ndb.Key('Template', json.dumps(content)).get()
      self.assertEqual(template_entity.content, content)

    # Create template entities with the same file again and check uniqueness.
    services.CreateTemplates(correct_blob['blob_info'])
    self.assertEqual(services.Template.query().count(), 2)

  def testErrorBlob(self):
    error_blob = self.error_blob
    # Test None when default template not indicated or found in templates.
    dflt_tmpl = services.CreateTemplates(error_blob['blob_info'])
    self.assertIsNone(dflt_tmpl)
