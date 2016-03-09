# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story


URL_LIST = [
    ('Polymer', 'http://todomvc.com/examples/polymer'),
    ('AngularJS', 'http://todomvc.com/examples/angularjs'),
    ('React', 'http://todomvc.com/examples/react'),
    ('Backbone.js', 'http://todomvc.com/examples/backbone'),
    ('Ember.js', 'http://todomvc.com/examples/emberjs'),
    ('Closure', 'http://todomvc.com/examples/closure'),
    ('GWT', 'http://todomvc.com/examples/gwt'),
    ('Dart', 'http://todomvc.com/examples/vanilladart/build/web'),
    ('Vanilla JS', 'http://todomvc.com/examples/vanillajs'),
]


class TodoMVCPage(page_module.Page):

  def __init__(self, url, page_set, name):
    super(TodoMVCPage, self).__init__(
        url=url, page_set=page_set, name=name,
        shared_page_state_class=shared_page_state.SharedDesktopPageState)

  def RunPageInteractions(self, action_runner):
    pass


class TodoMVCPageSet(story.StorySet):

  """ TodoMVC examples """

  def __init__(self):
    super(TodoMVCPageSet, self).__init__(
      archive_data_file='data/todomvc.json',
      cloud_storage_bucket=story.PUBLIC_BUCKET)

    for name, url in URL_LIST:
      self.AddStory(TodoMVCPage(url, self, name))
