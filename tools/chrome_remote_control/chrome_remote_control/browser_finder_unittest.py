# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest
import browser
import browser_options
import browser_finder
import os as real_os

# This file verifies the logic for finding a browser instance on all platforms
# at once. It does so by providing stubs for the OS/sys/subprocess primitives
# that the underlying finding logic usually uses to locate a suitable browser.
# We prefer this approach to having to run the same test on every platform on
# which we want this code to work.

class StubOSPath(object):
  def __init__(self, stub_os):
    self._os_stub = stub_os

  def exists(self, path):
    return path in self._os_stub.files

  def join(self, *args):
    return real_os.path.join(*args)

  def dirname(self, filename):
    return real_os.path.dirname(filename)

class StubOS(object):
  def __init__(self):
    self.path = StubOSPath(self)
    self.files = []

class StubSys(object):
  def __init__(self):
    self.platform = ""

class StubSubprocess(object):
  def __init__(self):
    self.call_hook = None

  def call(self, *args, **kwargs):
    if not self.call_hook:
      raise Exception("Should not be reached.")
    return self.call_hook(*args, **kwargs)

class FindTestBase(unittest.TestCase):
  def setUp(self):
    self._options = browser_options.BrowserOptions()
    self._options.chrome_root = "../../../"
    self._os_stub = StubOS()
    self._sys_stub = StubSys()
    self._subprocess_stub = StubSubprocess()

  @property
  def _files(self):
    return self._os_stub.files

  def DoFind(self):
    return browser_finder.FindBestPossibleBrowser(self._options,
        self._os_stub, self._sys_stub, self._subprocess_stub)

  def DoFindAll(self):
    return browser_finder.FindAllPossibleBrowsers(self._options,
        self._os_stub, self._sys_stub, self._subprocess_stub)

def has_type(array, type):
  return len([x for x in array if x.type == type]) != 0

class OSXFindTest(FindTestBase):
  def setUp(self):
    super(OSXFindTest, self).setUp()
    self._sys_stub.platform = 'darwin'
    self._files.append("/Applications/Google Chrome Canary.app/"
                       "Contents/MacOS/Google Chrome Canary")
    self._files.append("/Applications/Google Chrome.app/" +
                       "Contents/MacOS/Google Chrome")

  def testFindCanaryWithBothPresent(self):
    x = self.DoFind()
    self.assertEquals("canary", x.type)

  def testFind(self):
    ary = self.DoFindAll()
    self.assertTrue(has_type(ary, 'canary'))
    self.assertTrue(has_type(ary, 'system'))

    del self._files[0]
    ary = self.DoFindAll()
    self.assertFalse(has_type(ary, 'canary'))
    self.assertTrue(has_type(ary, 'system'))


class LinuxFindTest(FindTestBase):
  def setUp(self):
    super(LinuxFindTest, self).setUp()

    self._sys_stub.platform = 'linux2'
    self._files.append("/foo/chrome")
    self._files.append("../../../out/Release/chrome")
    self._files.append("../../../out/Debug/chrome")

    self._has_google_chrome_on_path = False
    this = self
    def call_hook(*args, **kwargs):
      if this._has_google_chrome_on_path:
        return 0
      raise OSError("Not found")
    self._subprocess_stub.call_hook = call_hook

  def testFindWithProvidedExecutable(self):
    self._options.browser_executable = "/foo/chrome"
    x = self.DoFind()
    self.assertEquals("exact", x.type)

  def testFindUsingDefaults(self):
    self._has_google_chrome_on_path = True
    x = self.DoFind()
    self.assertEquals("release", x.type)

    del self._files[1]
    self._has_google_chrome_on_path = True
    x = self.DoFind()
    self.assertEquals("system", x.type)

    self._has_google_chrome_on_path = False
    x = self.DoFind()
    self.assertEquals(x, None)

  def testFindUsingRelease(self):
    self._options.browser_types_to_use.append("debug")
    x = self.DoFind()
    self.assertEquals("release", x.type)

