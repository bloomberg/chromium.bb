# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import re

class Page(object):
  def __init__(self, url=None):
    self.url = url

  def __str__(self):
    return self.url

class PageSet(object):
  def __init__(self):
    self.pages = []
    self.description = ''

  def LoadFromFile(self, page_set_filename):
    with open(page_set_filename, 'r') as f:
      contents  = f.read()
      data = json.loads(contents)
      self.LoadFromDict(data)

  def LoadFromDict(self, data):
    self.description = data['description']
    for p in data['pages']:
      page = Page()
      for k, v in p.items():
        if k == 'url':
          if not re.match('(.+)://', v):
            v = 'http://%s' % v
        setattr(page, k, v)
      self.pages.append(page)

  def __iter__(self):
    return self.pages.__iter__()

