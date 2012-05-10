#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from cpp_type_generator import CppTypeGenerator
from json_schema import CachedLoad
import model
import unittest

class CppTypeGeneratorTest(unittest.TestCase):
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
    self.browser_action_json = CachedLoad('test/browserAction.json')
    self.model.AddNamespace(self.browser_action_json[0],
        'path/to/browserAction.json')
    self.browser_action = self.model.namespaces.get('browserAction')
    self.font_settings_json = CachedLoad('test/fontSettings.json')
    self.model.AddNamespace(self.font_settings_json[0],
        'path/to/fontSettings.json')
    self.font_settings = self.model.namespaces.get('fontSettings')
    self.dependency_tester_json = CachedLoad('test/dependencyTester.json')
    self.model.AddNamespace(self.dependency_tester_json[0],
        'path/to/dependencyTester.json')
    self.dependency_tester = self.model.namespaces.get('dependencyTester')

  def testGenerateIncludesAndForwardDeclarations(self):
    manager = CppTypeGenerator('', self.windows, self.windows.unix_name)
    manager.AddNamespace(self.tabs, self.tabs.unix_name)
    self.assertEquals('#include "path/to/tabs.h"',
        manager.GenerateIncludes().Render())
    self.assertEquals(
        'namespace tabs {\n'
        'struct Tab;\n'
        '}\n'
        'namespace windows {\n'
        'struct Window;\n'
        '}  // windows',
        manager.GenerateForwardDeclarations().Render())
    manager = CppTypeGenerator('', self.permissions, self.permissions.unix_name)
    self.assertEquals('', manager.GenerateIncludes().Render())
    self.assertEquals('namespace permissions {\n'
                      'struct Permissions;\n'
                      '}  // permissions',
                      manager.GenerateForwardDeclarations().Render())

  def testGenerateIncludesAndForwardDeclarationsMultipleTypes(self):
    m = model.Model()
    self.tabs_json[0]['types'].append(self.permissions_json[0]['types'][0])
    tabs_namespace = m.AddNamespace(self.tabs_json[0],
        'path/to/tabs.json')
    self.windows_json[0]['functions'].append(
        self.permissions_json[0]['functions'][1])
    windows = m.AddNamespace(self.windows_json[0],
        'path/to/windows.json')
    manager = CppTypeGenerator('', windows, self.windows.unix_name)
    manager.AddNamespace(tabs_namespace, self.tabs.unix_name)
    self.assertEquals('#include "path/to/tabs.h"',
        manager.GenerateIncludes().Render())
    self.assertEquals(
        'namespace tabs {\n'
        'struct Tab;\n'
        'struct Permissions;\n'
        '}\n'
        'namespace windows {\n'
        'struct Window;\n'
        '}  // windows',
        manager.GenerateForwardDeclarations().Render())

  def testGenerateIncludesAndForwardDeclarationsDependencies(self):
    m = model.Model()
    browser_action_namespace = m.AddNamespace(self.browser_action_json[0],
        'path/to/browserAction.json')
    font_settings_namespace = m.AddNamespace(self.font_settings_json[0],
        'path/to/fontSettings.json')
    manager = CppTypeGenerator('', self.dependency_tester,
        self.dependency_tester.unix_name)
    manager.AddNamespace(browser_action_namespace,
        self.browser_action.unix_name)
    manager.AddNamespace(font_settings_namespace,
        self.font_settings.unix_name)
    self.assertEquals('#include "path/to/browser_action.h"\n'
                      '#include "path/to/font_settings.h"',
                      manager.GenerateIncludes().Render())
    self.assertEquals(
        'namespace browserAction {\n'
        'typedef std::vector<int> ColorArray;\n'
        '}\n'
        'namespace fontSettings {\n'
        'typedef std::string ScriptCode;\n'
        '}\n'
        'namespace dependency_tester {\n'
        '}  // dependency_tester',
        manager.GenerateForwardDeclarations().Render())

  def testChoicesEnum(self):
    manager = CppTypeGenerator('', self.tabs, self.tabs.unix_name)
    prop = self.tabs.functions['move'].params[0]
    self.assertEquals('TAB_IDS_ARRAY',
        manager.GetEnumValue(prop, model.PropertyType.ARRAY.name))
    self.assertEquals('TAB_IDS_INTEGER',
        manager.GetEnumValue(prop, model.PropertyType.INTEGER.name))
    self.assertEquals('TabIdsType',
        manager.GetChoicesEnumType(prop))

  def testGetTypeSimple(self):
    manager = CppTypeGenerator('', self.tabs, self.tabs.unix_name)
    self.assertEquals('int',
        manager.GetType(
        self.tabs.types['Tab'].properties['id']))
    self.assertEquals('std::string',
        manager.GetType(
        self.tabs.types['Tab'].properties['status']))
    self.assertEquals('bool',
        manager.GetType(
        self.tabs.types['Tab'].properties['selected']))

  def testStringAsType(self):
    manager = CppTypeGenerator('', self.font_settings,
                               self.font_settings.unix_name)
    self.assertEquals('std::string',
        manager.GetType(
        self.font_settings.types['ScriptCode']))

  def testArrayAsType(self):
    manager = CppTypeGenerator('', self.browser_action,
                               self.browser_action.unix_name)
    self.assertEquals('std::vector<int>',
        manager.GetType(
        self.browser_action.types['ColorArray']))

  def testGetTypeArray(self):
    manager = CppTypeGenerator('', self.windows, self.windows.unix_name)
    self.assertEquals('std::vector<linked_ptr<Window> >',
        manager.GetType(
        self.windows.functions['getAll'].callback.params[0]))
    manager = CppTypeGenerator('', self.permissions, self.permissions.unix_name)
    self.assertEquals('std::vector<std::string>',
        manager.GetType(
        self.permissions.types['Permissions'].properties['origins']))

  def testGetTypeLocalRef(self):
    manager = CppTypeGenerator('', self.tabs, self.tabs.unix_name)
    self.assertEquals('Tab',
        manager.GetType(
        self.tabs.functions['get'].callback.params[0]))

  def testGetTypeIncludedRef(self):
    manager = CppTypeGenerator('', self.windows, self.windows.unix_name)
    manager.AddNamespace(self.tabs, self.tabs.unix_name)
    self.assertEquals('std::vector<linked_ptr<tabs::Tab> >',
        manager.GetType(
        self.windows.types['Window'].properties['tabs']))

  def testGetTypeNotfound(self):
    prop = self.windows.types['Window'].properties['tabs'].item_type
    prop.ref_type = 'Something'
    manager = CppTypeGenerator('', self.windows, self.windows.unix_name)
    self.assertRaises(KeyError, manager.GetType, prop)

  def testGetTypeNotimplemented(self):
    prop = self.windows.types['Window'].properties['tabs'].item_type
    prop.type_ = 10
    manager = CppTypeGenerator('', self.windows, self.windows.unix_name)
    self.assertRaises(NotImplementedError, manager.GetType, prop)

  def testGetTypeWithPadForGeneric(self):
    manager = CppTypeGenerator('', self.permissions, self.permissions.unix_name)
    self.assertEquals('std::vector<std::string> ',
        manager.GetType(
        self.permissions.types['Permissions'].properties['origins'],
        pad_for_generics=True))
    self.assertEquals('bool',
        manager.GetType(
        self.permissions.functions['contains'].callback.params[0],
        pad_for_generics=True))

  def testNamespaceDeclaration(self):
    manager = CppTypeGenerator('extensions', self.permissions,
                               self.permissions.unix_name)
    self.assertEquals(
        'namespace extensions {',
        manager.GetRootNamespaceStart().Render())

    manager = CppTypeGenerator('extensions::gen::api', self.permissions,
                               self.permissions.unix_name)
    self.assertEquals('namespace permissions {',
        manager.GetNamespaceStart().Render())
    self.assertEquals('}  // permissions',
        manager.GetNamespaceEnd().Render())
    self.assertEquals(
        'namespace extensions {\n'
        'namespace gen {\n'
        'namespace api {',
        manager.GetRootNamespaceStart().Render())
    self.assertEquals(
        '}  // api\n'
        '}  // gen\n'
        '}  // extensions',
        manager.GetRootNamespaceEnd().Render())

  def testExpandChoicesInParams(self):
    manager = CppTypeGenerator('extensions', self.tabs,
                               self.tabs.unix_name)
    props = self.tabs.functions['move'].params
    self.assertEquals(2, len(props))
    self.assertEquals(3, len(manager.GetExpandedChoicesInParams(props)))
    self.assertEquals(['move_properties', 'tab_ids_array', 'tab_ids_integer'],
        sorted([x.unix_name for x in manager.GetExpandedChoicesInParams(props)])
    )

if __name__ == '__main__':
  unittest.main()
