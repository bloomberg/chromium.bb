#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import idl_schema
import unittest

def getFunction(schema, name):
  for item in schema['functions']:
    if item['name'] == name:
      return item
  raise KeyError('Missing function %s' % name)

def getParams(schema, name):
  function = getFunction(schema, name)
  return function['parameters']

class IdlSchemaTest(unittest.TestCase):
  def setUp(self):
    loaded = idl_schema.Load('test/idl_basics.idl')
    self.assertEquals(1, len(loaded))
    self.assertEquals('idl_basics', loaded[0]['namespace'])
    self.idl_basics = loaded[0]

  def testSimpleCallbacks(self):
    schema = self.idl_basics
    expected = [{'type':'function', 'name':'Callback1', 'parameters':[]}]
    self.assertEquals(expected, getParams(schema, 'function4'))

    expected = [{'type':'function', 'name':'Callback2',
                 'parameters':[{'name':'x', 'type':'integer'}]}]
    self.assertEquals(expected, getParams(schema, 'function5'))

    expected = [{'type':'function', 'name':'Callback3',
                 'parameters':[{'name':'arg', '$ref':'idl_basics.MyType1'}]}]
    self.assertEquals(expected, getParams(schema, 'function6'))

  def testCallbackWithArrayArgument(self):
    schema = self.idl_basics
    expected = [{'type':'function', 'name':'Callback4',
                 'parameters':[{'name':'arg', 'type':'array',
                                'items':{'$ref':'idl_basics.MyType2'}}]}]
    self.assertEquals(expected, getParams(schema, 'function12'))


  def testArrayOfCallbacks(self):
    schema = idl_schema.Load('test/idl_callback_arrays.idl')[0]
    expected = [{'type':'array', 'name':'callbacks',
                 'items':{'type':'function', 'name':'MyCallback',
                          'parameters':[{'type':'integer', 'name':'x'}]}}]
    self.assertEquals(expected, getParams(schema, 'whatever'))

if __name__ == '__main__':
  unittest.main()
