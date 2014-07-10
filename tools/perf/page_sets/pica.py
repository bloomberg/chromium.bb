# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page_set as page_set_module
from measurements import polymer_load


class PicaPage(polymer_load.PageForPolymerLoad):

  def __init__(self, page_set):
    super(PicaPage, self).__init__(
      url='http://localhost/polymer/projects/pica/',
      page_set=page_set)
    self.archive_data_file = 'data/pica.json'


class PicaPageSet(page_set_module.PageSet):

  """ Pica demo app for the Polymer UI toolkit """

  def __init__(self):
    super(PicaPageSet, self).__init__(
      archive_data_file='data/pica.json')

    self.AddPage(PicaPage(self))
