#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json_schema
import json_schema_test
import unittest

class JsonSchemaUnittest(unittest.TestCase):
  def testNocompile(self):
    compiled = [
      {
        "namespace": "compile",
        "functions": [],
        "types":     {}
      },

      {
        "namespace": "functions",
        "functions": [
          {
            "id": "two"
          },
          {
            "id": "four"
          }
        ],

        "types": {
          "one": { "key": "value" }
        }
      },

      {
        "namespace": "types",
        "functions": [
          { "id": "one" }
        ],
        "types": {
          "two": {
            "key": "value"
          },
          "four": {
            "key": "value"
          }
        }
      },

      {
        "namespace": "nested",
        "properties": {
          "sync": {
            "functions": [
              {
                "id": "two"
              },
              {
                "id": "four"
              }
            ],
            "types": {
              "two": {
                "key": "value"
              },
              "four": {
                "key": "value"
              }
            }
          }
        }
      }
    ]

    schema = json_schema.CachedLoad('test/json_schema_test.json')
    self.assertEquals(compiled, json_schema.DeleteNodes(schema, 'nocompile'))

if __name__ == '__main__':
  unittest.main()
