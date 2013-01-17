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
    self.forbidden_json = CachedLoad('test/forbidden.json')
    self.model.AddNamespace(self.forbidden_json[0],
                            'path/to/forbidden.json')
    self.forbidden = self.model.namespaces.get('forbidden')
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
    self.browser_action_json = CachedLoad('test/browser_action.json')
    self.model.AddNamespace(self.browser_action_json[0],
                            'path/to/browser_action.json')
    self.browser_action = self.model.namespaces.get('browserAction')
    self.font_settings_json = CachedLoad('test/font_settings.json')
    self.model.AddNamespace(self.font_settings_json[0],
                            'path/to/font_settings.json')
    self.font_settings = self.model.namespaces.get('fontSettings')
    self.dependency_tester_json = CachedLoad('test/dependency_tester.json')
    self.model.AddNamespace(self.dependency_tester_json[0],
                            'path/to/dependency_tester.json')
    self.dependency_tester = self.model.namespaces.get('dependencyTester')
    self.content_settings_json = CachedLoad('test/content_settings.json')
    self.model.AddNamespace(self.content_settings_json[0],
                            'path/to/content_settings.json')
    self.content_settings = self.model.namespaces.get('contentSettings')

  def testGenerateIncludesAndForwardDeclarations(self):
    manager = CppTypeGenerator('', self.windows, self.windows.unix_name)
    manager.AddNamespace(self.tabs, self.tabs.unix_name)
    self.assertEquals('#include "path/to/tabs.h"\n'
                      '#include "base/string_number_conversions.h"\n'
                      '#include "base/json/json_writer.h"',
                      manager.GenerateIncludes().Render())
    self.assertEquals('namespace tabs {\n'
                      'struct Tab;\n'
                      '}\n'
                      'namespace windows {\n'
                      'struct Window;\n'
                      '}  // windows',
                      manager.GenerateForwardDeclarations().Render())
    manager = CppTypeGenerator('', self.permissions, self.permissions.unix_name)
    self.assertEquals('#include "base/string_number_conversions.h"\n'
                      '#include "base/json/json_writer.h"',
                      manager.GenerateIncludes().Render())
    self.assertEquals('namespace permissions {\n'
                      'struct Permissions;\n'
                      '}  // permissions',
                      manager.GenerateForwardDeclarations().Render())
    manager = CppTypeGenerator('', self.content_settings,
                               self.content_settings.unix_name)
    self.assertEquals('#include "base/string_number_conversions.h"',
                      manager.GenerateIncludes().Render())

  def testGenerateIncludesAndForwardDeclarationsDependencies(self):
    m = model.Model()
    # Insert 'font_settings' before 'browser_action' in order to test that
    # CppTypeGenerator sorts them properly.
    font_settings_namespace = m.AddNamespace(self.font_settings_json[0],
                                             'path/to/font_settings.json')
    browser_action_namespace = m.AddNamespace(self.browser_action_json[0],
                                              'path/to/browser_action.json')
    manager = CppTypeGenerator('', self.dependency_tester,
                               self.dependency_tester.unix_name)
    manager.AddNamespace(font_settings_namespace,
                         self.font_settings.unix_name)
    manager.AddNamespace(browser_action_namespace,
                         self.browser_action.unix_name)
    self.assertEquals('#include "path/to/browser_action.h"\n'
                      '#include "path/to/font_settings.h"\n'
                      '#include "base/string_number_conversions.h"',
                      manager.GenerateIncludes().Render())
    self.assertEquals('namespace browserAction {\n'
                      '}\n'
                      'namespace fontSettings {\n'
                      '}\n'
                      'namespace dependency_tester {\n'
                      '}  // dependency_tester',
                      manager.GenerateForwardDeclarations().Render())

  def testGetCppTypeSimple(self):
    manager = CppTypeGenerator('', self.tabs, self.tabs.unix_name)
    self.assertEquals(
        'int',
        manager.GetCppType(self.tabs.types['Tab'].properties['id'].type_))
    self.assertEquals(
        'std::string',
        manager.GetCppType(self.tabs.types['Tab'].properties['status'].type_))
    self.assertEquals(
        'bool',
        manager.GetCppType(self.tabs.types['Tab'].properties['selected'].type_))

  def testStringAsType(self):
    manager = CppTypeGenerator('', self.font_settings,
                               self.font_settings.unix_name)
    self.assertEquals(
        'std::string',
        manager.GetCppType(self.font_settings.types['FakeStringType']))

  def testArrayAsType(self):
    manager = CppTypeGenerator('', self.browser_action,
                               self.browser_action.unix_name)
    self.assertEquals(
        'std::vector<int>',
        manager.GetCppType(self.browser_action.types['ColorArray']))

  def testGetCppTypeArray(self):
    manager = CppTypeGenerator('', self.windows, self.windows.unix_name)
    self.assertEquals(
        'std::vector<linked_ptr<Window> >',
        manager.GetCppType(
            self.windows.functions['getAll'].callback.params[0].type_))
    manager = CppTypeGenerator('', self.permissions, self.permissions.unix_name)
    self.assertEquals(
        'std::vector<std::string>',
        manager.GetCppType(
            self.permissions.types['Permissions'].properties['origins'].type_))

  def testGetCppTypeLocalRef(self):
    manager = CppTypeGenerator('', self.tabs, self.tabs.unix_name)
    self.assertEquals(
        'Tab',
        manager.GetCppType(self.tabs.functions['get'].callback.params[0].type_))

  def testGetCppTypeIncludedRef(self):
    manager = CppTypeGenerator('', self.windows, self.windows.unix_name)
    manager.AddNamespace(self.tabs, self.tabs.unix_name)
    self.assertEquals(
        'std::vector<linked_ptr<tabs::Tab> >',
        manager.GetCppType(
            self.windows.types['Window'].properties['tabs'].type_))

  def testGetCppTypeWithPadForGeneric(self):
    manager = CppTypeGenerator('', self.permissions, self.permissions.unix_name)
    self.assertEquals('std::vector<std::string>',
        manager.GetCppType(
            self.permissions.types['Permissions'].properties['origins'].type_,
            is_in_container=False))
    self.assertEquals('linked_ptr<std::vector<std::string> >',
        manager.GetCppType(
            self.permissions.types['Permissions'].properties['origins'].type_,
            is_in_container=True))
    self.assertEquals('bool',
        manager.GetCppType(
            self.permissions.functions['contains'].callback.params[0].type_,
        is_in_container=True))

  def testNamespaceDeclaration(self):
    manager = CppTypeGenerator('extensions', self.permissions,
                               self.permissions.unix_name)
    self.assertEquals('namespace extensions {',
                      manager.GetRootNamespaceStart().Render())

    manager = CppTypeGenerator('extensions::gen::api', self.permissions,
                               self.permissions.unix_name)
    self.assertEquals('namespace permissions {',
                      manager.GetNamespaceStart().Render())
    self.assertEquals('}  // permissions',
                      manager.GetNamespaceEnd().Render())
    self.assertEquals('namespace extensions {\n'
                      'namespace gen {\n'
                      'namespace api {',
                      manager.GetRootNamespaceStart().Render())
    self.assertEquals('}  // api\n'
                      '}  // gen\n'
                      '}  // extensions',
                      manager.GetRootNamespaceEnd().Render())

if __name__ == '__main__':
  unittest.main()
