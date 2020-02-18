# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test cros_extract_deps."""

from __future__ import print_function

from chromite.lib import cros_test_lib
from chromite.scripts import cros_extract_deps


class FlattenDepTreeTest(cros_test_lib.TestCase):
  """Tests for cros_extract_deps.FlattenDepTree."""

  def testFlattenDepTreeSimple(self):
    dep_tree = {
        'deathstar/darthvader-2.3': {
            'action': 'merge',
            'deps': {
                'deathstar/trooper-1.2': {
                    'action': 'merge',
                    'deps': {
                        'weapon/blasterpistol-2.1': {
                            'action': 'merge',
                            'deps': {},
                            'deptype': 'runtime',
                        }
                    },
                    'deptype': 'runtime',
                },
                'deathstar/pilot-2.3': {
                    'action': 'merge',
                    'deps': {},
                    'deptype': 'runtime',
                },
                'deathstar/commander-2.3': {
                    'action': 'merge',
                    'deps': {},
                    'deptype': 'runtime',
                },
            },
        },
    }
    flatten_dep_tree = {
        'weapon/blasterpistol-2.1': {
            'rev_deps': ['deathstar/trooper-1.2'],
            'category': 'weapon',
            'version': '2.1',
            'name': 'blasterpistol',
            'deps': [],
            'action': 'merge',
            'cpes': [],
            'full_name': 'weapon/blasterpistol-2.1',
        },
        'deathstar/darthvader-2.3': {
            'rev_deps': [],
            'category': 'deathstar',
            'version': '2.3',
            'name': 'darthvader',
            'deps': [
                'deathstar/commander-2.3', 'deathstar/pilot-2.3',
                'deathstar/trooper-1.2',
            ],
            'action': 'merge',
            'cpes': [],
            'full_name': 'deathstar/darthvader-2.3',
        },
        'deathstar/pilot-2.3': {
            'rev_deps': ['deathstar/darthvader-2.3'],
            'category': 'deathstar',
            'version': '2.3',
            'name': 'pilot',
            'deps': [],
            'action': 'merge',
            'cpes': [],
            'full_name': 'deathstar/pilot-2.3',
        },
        'deathstar/commander-2.3': {
            'rev_deps': ['deathstar/darthvader-2.3'],
            'category': 'deathstar',
            'version': '2.3',
            'name': 'commander',
            'deps': [],
            'action': 'merge',
            'cpes': [],
            'full_name': 'deathstar/commander-2.3',
        },
        'deathstar/trooper-1.2': {
            'rev_deps': ['deathstar/darthvader-2.3'],
            'category': 'deathstar',
            'version': '1.2',
            'name': 'trooper',
            'deps': ['weapon/blasterpistol-2.1'],
            'action': 'merge',
            'cpes': [],
            'full_name': 'deathstar/trooper-1.2',
        }
    }
    self.assertEqual(
        cros_extract_deps.FlattenDepTree(dep_tree), flatten_dep_tree)
