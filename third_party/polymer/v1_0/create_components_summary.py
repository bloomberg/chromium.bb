# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import json

COMPONENTS_DIR = 'components-chromium'
COMPONENT_SUMMARY =\
"""Name: %(name)s
Version: %(version)s
Repository: %(repository)s
Tag: %(tag)s
Revision: %(revision)s
Tree link: %(tree)s
"""

for entry in sorted(os.listdir(COMPONENTS_DIR)):
  component_path = os.path.join(COMPONENTS_DIR, entry)
  if not os.path.isdir(component_path):
    continue
  bower_path = os.path.join(component_path, '.bower.json')
  if not os.path.isfile(bower_path):
    raise Exception('%s is not a file.' % bower_path)
  with open(bower_path) as stream:
    info = json.load(stream)
  repository = info['_source']
  tree = 'https%s/tree/%s' % (repository[3:-4], info['_resolution']['tag'])
  print COMPONENT_SUMMARY % {
    'name': info['name'],
    'version': info['version'],
    'repository': repository,
    'tag': info['_resolution']['tag'],
    'revision': info['_resolution']['commit'],
    'tree': tree
  }
