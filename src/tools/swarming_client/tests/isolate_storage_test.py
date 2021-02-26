#!/usr/bin/env vpython3
# Copyright 2014 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import hashlib
import unittest

# Mutates sys.path.
import test_env

import isolate_storage


class HashAlgoNameTest(unittest.TestCase):

  def test_get_hash_algo(self):
    pairs = [
      ('default', hashlib.sha1),
      ('default-gzip', hashlib.sha1),
      ('sha1-flat', hashlib.sha1),
      ('sha1-deflate', hashlib.sha1),
      ('sha256-flat', hashlib.sha256),
      ('sha256-deflate', hashlib.sha256),
      ('sha512-flat', hashlib.sha512),
      ('sha512-deflate', hashlib.sha512),
    ]
    for namespace, expected in pairs:
      server_ref = isolate_storage.ServerRef('http://localhost:0', namespace)
      self.assertIs(expected, server_ref.hash_algo, namespace)


if __name__ == '__main__':
  test_env.main()
