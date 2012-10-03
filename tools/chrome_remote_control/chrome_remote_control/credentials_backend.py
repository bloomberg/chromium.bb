# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from chrome_remote_control import util

class CredentialsBackend(object):
  label = None

  def __init__(self, browser_credentials):
    self.browser_credentials = browser_credentials

  def LoginNeeded(self, tab):
    raise NotImplementedError()

  def LoginNoLongerNeeded(self, tab):
    raise NotImplementedError()

  @staticmethod
  def SubmitForm(form_id, tab):
    """Submits the given form ID, and returns after it has been submitted.

    Args:
      form_id: the id attribute of the form to submit.
    """
    js = 'document.getElementById("%s").submit();' % form_id

    tab.runtime.Execute(js)

  @staticmethod
  def WaitForFormToLoad(form_id, tab):
    def IsFormLoaded():
      return tab.runtime.Evaluate(
          'document.querySelector("#%s")!== null' % form_id)

    # Wait until the form is submitted and the page completes loading.
    util.WaitFor(lambda: IsFormLoaded(), 60) # pylint: disable=W0108

  @classmethod
  def SubmitFormAndWait(cls, form_id, tab):
    cls.SubmitForm(form_id, tab)

    def IsLoginStillHappening():
      return tab.runtime.Evaluate(
          'document.querySelector("#%s")!== null' % form_id)

    # Wait until the form is submitted and the page completes loading.
    util.WaitFor(lambda: not IsLoginStillHappening(), 60)
