# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page_set as page_set_module
from measurements import polymer_load


class TopekaPage(polymer_load.PageForPolymerLoad):

  def __init__(self, page_set):
    super(TopekaPage, self).__init__(
      url='http://www.polymer-project.org/apps/topeka/?test',
      ready_event='template-bound',
      page_set=page_set)
    self.archive_data_file = 'data/topeka.json'


class TopekaPageSet(page_set_module.PageSet):

  """ Topeka quiz app for the Polymer UI toolkit """

  def __init__(self):
    super(TopekaPageSet, self).__init__(
      user_agent_type='mobile',
      archive_data_file='data/topeka.json')

    self.AddPage(TopekaPage(self))
