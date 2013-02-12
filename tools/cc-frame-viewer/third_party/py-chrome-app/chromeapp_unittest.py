# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import shutil
import tempfile
import unittest

import chromeapp

unittest_data_dir = os.path.relpath(
  os.path.join(
    os.path.dirname(__file__),
    'unittest_data'))

class TrackingAppInstance(chromeapp.AppInstance):
  def __init__(self, *args):
    super(TrackingAppInstance, self).__init__(*args)
    self.did_install = False

  def _Install(self, *args):
    self.did_install = True
    return super(TrackingAppInstance, self)._Install(*args)

class AppTest(unittest.TestCase):
  def setUp(self):
    self._profiles_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self._profiles_dir, ignore_errors=True)

  def testBasic(self):
    manifest_file = os.path.join(unittest_data_dir,
                                 'simple-working-app', 'manifest.json')
    app = chromeapp.App('simple-working-app',
                       manifest_file,
                       chromeapp_profiles_dir=self._profiles_dir)
    with TrackingAppInstance(app, ['hello']) as app_instance:
      ret = app_instance.Run()
    self.assertEquals(ret, 42)

  def testLaunchAndLaunchAgain(self):
    manifest_file = os.path.join(unittest_data_dir,
                                 'simple-working-app', 'manifest.json')
    app = chromeapp.App('simple-working-app',
                       manifest_file,
                       chromeapp_profiles_dir=self._profiles_dir)
    with TrackingAppInstance(app, ['hello']) as app_instance:
      ret = app_instance.Run()
    self.assertEquals(ret, 42)

    with TrackingAppInstance(app, ['hello']) as app_instance:
      ret = app_instance.Run()
      assert app_instance.did_install == False
    self.assertEquals(ret, 42)

  def testAppSideUncaughtErrorObject(self):
    manifest_file = os.path.join(unittest_data_dir,
                                 'intentionally-failing-app', 'manifest.json')
    app = chromeapp.App('intentionally-failing-app',
                       manifest_file,
                       chromeapp_profiles_dir=self._profiles_dir)
    test = self
    class MyAppInstance(chromeapp.AppInstance):
      def _OnUncaughtError(self, error):
        try:
          test.assertEquals(error['error'], 'Uncaught Error: intentional failure')
        finally:
          self.ExitRunLoop(0)
    with MyAppInstance(app, '--throw-error-object') as app_instance:
      ret = app_instance.Run()

  def testAppThatPrints(self):
    manifest_file = os.path.join(unittest_data_dir,
                                 'app-that-prints', 'manifest.json')
    app = chromeapp.App('app-that-prints',
                       manifest_file,
                       chromeapp_profiles_dir=self._profiles_dir)
    test = self
    class MyAppInstance(chromeapp.AppInstance):
      def _OnPrint(self, contents):
        try:
          test.assertEquals(len(contents), 1)
          test.assertEquals(contents[0], 'Hello world')
        finally:
          self.ExitRunLoop(0)
    with MyAppInstance(app) as app_instance:
      ret = app_instance.Run()

  def testAppThatSendsEvent(self):
    manifest_file = os.path.join(unittest_data_dir,
                                 'app-that-sends-event', 'manifest.json')
    app = chromeapp.App('app-that-sends-event',
                        manifest_file,
                        chromeapp_profiles_dir=self._profiles_dir)
    got_event = [False]
    def OnEvent(args):
      arg1, arg2 = args
      self.assertEquals(arg1, [1, 2, 3])
      self.assertEquals(arg2, True)
      got_event[0] = True
      return [314, 'hi']

    with chromeapp.AppInstance(app) as app_instance:
      self.assertFalse(app_instance.HasListener('hello-world', OnEvent))
      app_instance.AddListener('hello-world', OnEvent)
      self.assertTrue(app_instance.HasListener('hello-world', OnEvent))
      ret = app_instance.Run()
      self.assertEquals(0, ret)
      app_instance.RemoveListener('hello-world', OnEvent)
    self.assertTrue(got_event[0])
