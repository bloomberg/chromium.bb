# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is expected to be used under another directory to use,
# so we disable checking import path of GAE tools from this directory.
# pylint: disable=F0401,E0611,W0232

import jinja2
import json
import os
import re
import urllib
import webapp2

from google.appengine.ext import blobstore
from google.appengine.ext.webapp import blobstore_handlers

import services


JINJA_ENVIRONMENT = jinja2.Environment(
  loader=jinja2.FileSystemLoader(os.path.dirname(__file__)),
  extensions=['jinja2.ext.autoescape'])


class MainPage(webapp2.RequestHandler):
  """Show breakdown with received profiler-id and template-id. If nothing was
  received, show blank page waiting user to upload file."""
  def get(self):
    page_template = JINJA_ENVIRONMENT.get_template('index.html')
    upload_url = blobstore.create_upload_url('/upload')

    # Get profiler id and template id from url query.
    run_id = self.request.get('run_id')
    tmpl_id = self.request.get('tmpl_id')
    upload_msg = self.request.get('upload_msg')

    template_values = {
      'upload_url': upload_url,
      'upload_msg': upload_msg
    }

    if run_id and tmpl_id:
      template_values['json'] = services.GetProfiler(run_id)
      template_values['template'] = services.GetTemplate(tmpl_id)

    self.response.write(page_template.render(template_values))


class UploadHandler(blobstore_handlers.BlobstoreUploadHandler):
  """Handle file uploading with BlobstoreUploadHandler. BlobstoreUploadHandler
  can deal with files overweighing size limitation within one HTTP connection so
  that user can upload large json file."""
  def post(self):
    blob_info = self.get_uploads('file')[0]

    run_id = services.CreateProfiler(blob_info)
    default_key = services.CreateTemplates(blob_info)

    # TODO(junjianx): Validation of uploaded file should be done separately.
    if not default_key:
      # Jump to home page with error message.
      req_params = {
        'upload_msg': 'No default_template key was found.'
      }
    else:
      # Jump to new graph page using default template.
      req_params = {
        'run_id': run_id,
        'tmpl_id': default_key.urlsafe()
      }

    self.redirect('/?' + urllib.urlencode(req_params))


class ShareHandler(webapp2.RequestHandler):
  """Handle breakdown template sharing. Generate public url for transferred
  template and return it back."""
  def post(self):
    run_id = self.request.POST['run_id']
    content = json.loads(self.request.POST['content'])
    tmpl_key = services.CreateTemplate(content)

    req_params = {
      'run_id': run_id,
      'tmpl_id': tmpl_key.urlsafe()
    }

    # Take out host url from request by removing share suffix.
    url = re.sub('share', '', self.request.url)
    self.response.write(url + '?' + urllib.urlencode(req_params))


application = webapp2.WSGIApplication([
  ('/', MainPage),
  ('/upload', UploadHandler),
  ('/share', ShareHandler)
], debug=True)
