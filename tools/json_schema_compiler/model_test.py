# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import model
import unittest

class ModelTest(unittest.TestCase):
  def setUp(self):
    self.model = model.Model()
    self.permissions_json = json.loads(open('test/permissions.json').read())
    self.model.AddNamespace(self.permissions_json[0],
        'path/to/permissions.json')
    self.permissions = self.model.namespaces.get('permissions')
    self.windows_json = json.loads(open('test/windows.json').read())
    self.model.AddNamespace(self.windows_json[0],
        'path/to/window.json')
    self.windows = self.model.namespaces.get('windows')
    self.tabs_json = json.loads(open('test/tabs.json').read())
    self.model.AddNamespace(self.tabs_json[0],
        'path/to/tabs.json')
    self.tabs = self.model.namespaces.get('tabs')

  def testNamespaces(self):
    self.assertEquals(3, len(self.model.namespaces))
    self.assertTrue(self.permissions)

  def testNamespaceNocompile(self):
    self.permissions_json[0]['namespace'] = 'something'
    del self.permissions_json[0]['compile']
    self.model.AddNamespace(self.permissions_json[0],
        'path/to/something.json')
    self.assertEquals(3, len(self.model.namespaces))

  def testHasFunctions(self):
    self.assertEquals(["contains", "getAll", "remove", "request"],
        sorted(self.permissions.functions.keys()))

  def testFunctionNoCallback(self):
    del (self.permissions_json[0]['functions'][0]['parameters'][0])
    self.assertRaises(AssertionError, self.model.AddNamespace,
        self.permissions_json[0], 'path/to/something.json')

  def testFunctionNocompile(self):
    # tabs.json has 2 functions marked as nocompile (connect, sendRequest)
    self.assertEquals(["captureVisibleTab", "create", "detectLanguage",
          "executeScript", "get", "getAllInWindow", "getCurrent",
          "getSelected", "highlight", "insertCSS", "move", "query", "reload",
          "remove", "update"],
          sorted(self.tabs.functions.keys())
    )

  def testHasTypes(self):
    self.assertEquals(['Tab'], self.tabs.types.keys())
    self.assertEquals(['Permissions'], self.permissions.types.keys())
    self.assertEquals(['Window'], self.windows.types.keys())

  def testHasProperties(self):
    self.assertEquals(["active", "favIconUrl", "highlighted", "id",
        "incognito", "index", "pinned", "selected", "status", "title", "url",
        "windowId"],
        sorted(self.tabs.types['Tab'].properties.keys()))

  def testProperties(self):
    string_prop = self.tabs.types['Tab'].properties['status']
    self.assertEquals(model.PropertyType.STRING, string_prop.type_)
    integer_prop = self.tabs.types['Tab'].properties['id']
    self.assertEquals(model.PropertyType.INTEGER, integer_prop.type_)
    array_prop = self.windows.types['Window'].properties['tabs']
    self.assertEquals(model.PropertyType.ARRAY, array_prop.type_)
    self.assertEquals(model.PropertyType.REF, array_prop.item_type.type_)
    self.assertEquals('Tab', array_prop.item_type.ref_type)
    object_prop = self.tabs.functions['query'].params[0]
    self.assertEquals(model.PropertyType.OBJECT, object_prop.type_)
    self.assertEquals(
        ["active", "highlighted", "pinned", "status", "title", "url",
         "windowId", "windowType"],
        sorted(object_prop.properties.keys()))
    # TODO(calamity): test choices

  def testPropertyNotImplemented(self):
    (self.permissions_json[0]['types'][0]
        ['properties']['permissions']['type']) = 'something'
    self.assertRaises(NotImplementedError, self.model.AddNamespace,
        self.permissions_json[0], 'path/to/something.json')

  def testDescription(self):
    self.assertFalse(
        self.permissions.functions['contains'].params[0].description)
    self.assertEquals('True if the extension has the specified permissions.',
        self.permissions.functions['contains'].callback.param.description)

if __name__ == '__main__':
  unittest.main()
