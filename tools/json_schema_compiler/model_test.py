#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from json_schema import CachedLoad
import model
import unittest

class ModelTest(unittest.TestCase):
  def setUp(self):
    self.model = model.Model()
    self.permissions_json = CachedLoad('test/permissions.json')
    self.model.AddNamespace(self.permissions_json[0],
        'path/to/permissions.json')
    self.permissions = self.model.namespaces.get('permissions')
    self.windows_json = CachedLoad('test/windows.json')
    self.model.AddNamespace(self.windows_json[0],
        'path/to/window.json')
    self.windows = self.model.namespaces.get('windows')
    self.tabs_json = CachedLoad('test/tabs.json')
    self.model.AddNamespace(self.tabs_json[0],
        'path/to/tabs.json')
    self.tabs = self.model.namespaces.get('tabs')

  def testNamespaces(self):
    self.assertEquals(3, len(self.model.namespaces))
    self.assertTrue(self.permissions)

  def testHasFunctions(self):
    self.assertEquals(["contains", "getAll", "remove", "request"],
        sorted(self.permissions.functions.keys()))

  def testHasTypes(self):
    self.assertEquals(['tabs.Tab'], self.tabs.types.keys())
    self.assertEquals(['permissions.Permissions'],
        self.permissions.types.keys())
    self.assertEquals(['windows.Window'], self.windows.types.keys())

  def testHasProperties(self):
    self.assertEquals(["active", "favIconUrl", "highlighted", "id",
        "incognito", "index", "pinned", "selected", "status", "title", "url",
        "windowId"],
        sorted(self.tabs.types['tabs.Tab'].properties.keys()))

  def testProperties(self):
    string_prop = self.tabs.types['tabs.Tab'].properties['status']
    self.assertEquals(model.PropertyType.STRING, string_prop.type_)
    integer_prop = self.tabs.types['tabs.Tab'].properties['id']
    self.assertEquals(model.PropertyType.INTEGER, integer_prop.type_)
    array_prop = self.windows.types['windows.Window'].properties['tabs']
    self.assertEquals(model.PropertyType.ARRAY, array_prop.type_)
    self.assertEquals(model.PropertyType.REF, array_prop.item_type.type_)
    self.assertEquals('tabs.Tab', array_prop.item_type.ref_type)
    object_prop = self.tabs.functions['query'].params[0]
    self.assertEquals(model.PropertyType.OBJECT, object_prop.type_)
    self.assertEquals(
        ["active", "highlighted", "pinned", "status", "title", "url",
         "windowId", "windowType"],
        sorted(object_prop.properties.keys()))

  def testChoices(self):
    self.assertEquals(model.PropertyType.CHOICES,
        self.tabs.functions['move'].params[0].type_)

  def testPropertyNotImplemented(self):
    (self.permissions_json[0]['types'][0]
        ['properties']['permissions']['type']) = 'something'
    self.assertRaises(model.ParseException, self.model.AddNamespace,
        self.permissions_json[0], 'path/to/something.json')

  def testDescription(self):
    self.assertFalse(
        self.permissions.functions['contains'].params[0].description)
    self.assertEquals('True if the extension has the specified permissions.',
        self.permissions.functions['contains'].callback.params[0].description)

  def testPropertyUnixName(self):
    param = self.tabs.functions['move'].params[0]
    self.assertEquals('tab_ids', param.unix_name)
    self.assertRaises(AttributeError,
        param.choices[model.PropertyType.INTEGER].GetUnixName)
    param.choices[model.PropertyType.INTEGER].unix_name = 'asdf'
    param.choices[model.PropertyType.INTEGER].unix_name = 'tab_ids_integer'
    self.assertEquals('tab_ids_integer',
        param.choices[model.PropertyType.INTEGER].unix_name)
    self.assertRaises(AttributeError,
        param.choices[model.PropertyType.INTEGER].SetUnixName, 'breakage')

  def testUnixName(self):
    expectations = {
      'foo': 'foo',
      'fooBar': 'foo_bar',
      'fooBarBaz': 'foo_bar_baz',
      'fooBARBaz': 'foo_bar_baz',
      'fooBAR': 'foo_bar',
      'FOO': 'foo',
      'FOOBar': 'foo_bar',
      'foo.bar': 'foo_bar',
      'foo.BAR': 'foo_bar',
      'foo.barBAZ': 'foo_bar_baz'
      }
    for name in expectations:
      self.assertEquals(expectations[name], model._UnixName(name));

if __name__ == '__main__':
  unittest.main()
