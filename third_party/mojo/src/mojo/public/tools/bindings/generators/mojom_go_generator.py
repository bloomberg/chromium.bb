# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Generates Go source files from a mojom.Module.'''

import os

from mojom.generate.template_expander import UseJinja

import mojom.generate.generator as generator
import mojom.generate.module as mojom

def GetPackage(module):
  if module.namespace:
    return module.namespace.split('.')[-1]
  return 'mojom'

def GetPackagePath(module):
  path = 'mojom'
  for i in module.namespace.split('.'):
    path = os.path.join(path, i)
  return path

class Generator(generator.Generator):
  go_filters = {}

  def GetParameters(self):
    return {
      'package': GetPackage(self.module),
    }

  @UseJinja('go_templates/source.tmpl', filters=go_filters)
  def GenerateSource(self):
    return self.GetParameters()

  def GenerateFiles(self, args):
    self.Write(self.GenerateSource(), os.path.join("go", "src", "gen",
        GetPackagePath(self.module), '%s.go' % self.module.name))

  def GetJinjaParameters(self):
    return {
      'lstrip_blocks': True,
      'trim_blocks': True,
    }

  def GetGlobals(self):
    return {
      'namespace': self.module.namespace,
      'module': self.module,
    }
