# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class PicaPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, page_set):
    super(PicaPage, self).__init__(
      url='http://localhost/polymer/projects/pica/',
      page_set=page_set)
    self.archive_data_file = 'data/pica.json'
    self.script_to_evaluate_on_commit = '''
    document.addEventListener('polymer-ready', function() {
      var unused = document.body.offsetHeight;
      window.__pica_load_time = performance.now();
      setTimeout(function(){window.__polymer_ready=true}, 1000)
    })'''

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'javascript': 'window.__polymer_ready'
      }))


class PicaPageSet(page_set_module.PageSet):

  """ Pica demo app for the Polymer UI toolkit """

  def __init__(self):
    super(PicaPageSet, self).__init__(
      archive_data_file='data/pica.json')

    self.AddPage(PicaPage(self))
