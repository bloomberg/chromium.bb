#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import schema_util
import unittest

class SchemaUtilTest(unittest.TestCase):
  def testStripSchemaNamespace(self):
    self.assertEquals('Bar', schema_util.StripSchemaNamespace('foo.Bar'))
    self.assertEquals('Baz', schema_util.StripSchemaNamespace('Baz'))

  def testPrefixSchemasWithNamespace(self):
    schemas = [
        { 'namespace': 'n1',
          'types': [
            {
              'id': 'T1',
              'customBindings': 'T1',
              'properties': {
                'p1': {'$ref': 'T1'},
                'p2': {'$ref': 'fully.qualified.T'},
              }
            }
          ],
          'functions': [
            {
              'parameters': [
                { '$ref': 'T1' },
                { '$ref': 'fully.qualified.T' },
              ],
              'returns': { '$ref': 'T1' }
            },
          ],
          'events': [
            {
              'parameters': [
                { '$ref': 'T1' },
                { '$ref': 'fully.qualified.T' },
              ],
            },
          ],
        },
      ]
    schema_util.PrefixSchemasWithNamespace(schemas)
    self.assertEquals('n1.T1', schemas[0]['types'][0]['id'])
    self.assertEquals('n1.T1', schemas[0]['types'][0]['customBindings'])
    self.assertEquals('n1.T1',
        schemas[0]['types'][0]['properties']['p1']['$ref'])
    self.assertEquals('fully.qualified.T',
        schemas[0]['types'][0]['properties']['p2']['$ref'])

    self.assertEquals('n1.T1',
        schemas[0]['functions'][0]['parameters'][0]['$ref'])
    self.assertEquals('fully.qualified.T',
        schemas[0]['functions'][0]['parameters'][1]['$ref'])
    self.assertEquals('n1.T1',
        schemas[0]['functions'][0]['returns']['$ref'])

    self.assertEquals('n1.T1',
        schemas[0]['events'][0]['parameters'][0]['$ref'])
    self.assertEquals('fully.qualified.T',
        schemas[0]['events'][0]['parameters'][1]['$ref'])

if __name__ == '__main__':
  unittest.main()
