# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from cpp_type_generator import CppTypeGenerator
import json
import model
import unittest

class CppTypeGeneratorTest(unittest.TestCase):
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

  def testGenerateCppIncludes(self):
    manager = CppTypeGenerator('', self.windows, 'windows_api')
    manager.AddNamespace(self.tabs, 'tabs_api')
    self.assertEquals('#include "path/to/tabs_api.h"',
        manager.GenerateCppIncludes().Render())
    manager = CppTypeGenerator('', self.permissions, 'permissions_api')
    manager.AddNamespace(self.permissions, 'permissions_api')
    self.assertEquals('', manager.GenerateCppIncludes().Render())

  def testGenerateCppIncludesMultipleTypes(self):
    m = model.Model()
    self.tabs_json[0]['types'].append(self.permissions_json[0]['types'][0])
    tabs_namespace = m.AddNamespace(self.tabs_json[0],
        'path/to/tabs.json')
    self.windows_json[0]['functions'].append(
        self.permissions_json[0]['functions'][1])
    windows = m.AddNamespace(self.windows_json[0],
        'path/to/windows.json')
    manager = CppTypeGenerator('', windows, 'windows_api')
    manager.AddNamespace(tabs_namespace, 'tabs_api')
    self.assertEquals('#include "path/to/tabs_api.h"',
        manager.GenerateCppIncludes().Render())

  def testGetTypeSimple(self):
    manager = CppTypeGenerator('', self.tabs, 'tabs_api')
    self.assertEquals('int',
        manager.GetType(
        self.tabs.types['Tab'].properties['id']))
    self.assertEquals('std::string',
        manager.GetType(
        self.tabs.types['Tab'].properties['status']))
    self.assertEquals('bool',
        manager.GetType(
        self.tabs.types['Tab'].properties['selected']))

  def testGetTypeArray(self):
    manager = CppTypeGenerator('', self.windows, 'windows_api')
    self.assertEquals('std::vector<Window>',
        manager.GetType(
        self.windows.functions['getAll'].callback.param))
    manager = CppTypeGenerator('', self.permissions, 'permissions_api')
    self.assertEquals('std::vector<std::string>',
        manager.GetType(
        self.permissions.types['Permissions'].properties['origins']))

  def testGetTypeLocalRef(self):
    manager = CppTypeGenerator('', self.tabs, 'tabs_api')
    self.assertEquals('Tab',
        manager.GetType(
        self.tabs.functions['get'].callback.param))

  def testGetTypeIncludedRef(self):
    manager = CppTypeGenerator('', self.windows, 'windows_api')
    manager.AddNamespace(self.tabs, 'tabs_api')
    self.assertEquals('std::vector<tabs_api::Tab>',
        manager.GetType(
        self.windows.types['Window'].properties['tabs']))

  def testGetTypeNotfound(self):
    prop = self.windows.types['Window'].properties['tabs'].item_type
    prop.ref_type = 'Something'
    manager = CppTypeGenerator('', self.windows, 'windows_api')
    self.assertRaises(KeyError, manager.GetType, prop)

  def testGetTypeNotimplemented(self):
    prop = self.windows.types['Window'].properties['tabs'].item_type
    prop.type_ = 10
    manager = CppTypeGenerator('', self.windows, 'windows_api')
    self.assertRaises(NotImplementedError, manager.GetType, prop)

  def testGetTypeWithPadForGeneric(self):
    manager = CppTypeGenerator('', self.permissions, 'permissions_api')
    self.assertEquals('std::vector<std::string> ',
        manager.GetType(
        self.permissions.types['Permissions'].properties['origins'],
        pad_for_generics=True))
    self.assertEquals('bool',
        manager.GetType(
        self.permissions.functions['contains'].callback.param,
        pad_for_generics=True))

  def testNamespaceDeclaration(self):
    manager = CppTypeGenerator('extensions', self.permissions,
                               'permissions_api')
    self.assertEquals(
        'namespace extensions {\n'
        'namespace permissions_api {',
        manager.GetCppNamespaceStart().Render())

    manager = CppTypeGenerator('extensions::gen::api', self.permissions,
                               'permissions_api')
    self.assertEquals(
        'namespace extensions {\n'
        'namespace gen {\n'
        'namespace api {\n'
        'namespace permissions_api {',
        manager.GetCppNamespaceStart().Render())

if __name__ == '__main__':
  unittest.main()
