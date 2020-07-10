# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the field_mask_util.py module."""

from __future__ import print_function

from google.protobuf import field_mask_pb2

from chromite.lib import cros_test_lib
from chromite.utils import field_mask_util


class CreateFilteredDictTest(cros_test_lib.TestCase):
  """Unit tests for CreateFilteredDict"""

  def testCreateFilteredDictWithFieldAndSubField(self):
    """Tests selecting top-level fields and sub-fields."""
    original_dict = {
        'a': {
            'b': {
                'c': 'val1',
                'd': 2,
            }
        },
        'e': {
            'f': {
                'g': 'val2',
                'h': 3,
            }
        },
        'i': 3,
        'j': 'val3',
    }

    field_mask = field_mask_pb2.FieldMask(paths=['a.b.c', 'e.f', 'i'])

    expected_dict = {
        'a': {
            'b': {
                'c': 'val1',
            }
        },
        'e': {
            'f': {
                'g': 'val2',
                'h': 3,
            }
        },
        'i': 3,
    }

    merged_dict = field_mask_util.CreateFilteredDict(field_mask, original_dict)
    self.assertDictEqual(expected_dict, merged_dict)

  def testCreateFilteredDictWithOverlappingSubFields(self):
    """Tests selecting overlapping sub-fields."""
    original_dict = {
        'a': {
            'b': {
                'c': 'val1',
                'd': 2,
                'e': 3,
            },
        },
    }

    field_mask = field_mask_pb2.FieldMask(paths=['a.b.c', 'a.b.e'])

    expected_dict = {
        'a': {
            'b': {
                'c': 'val1',
                'e': 3,
            },
        },
    }

    merged_dict = field_mask_util.CreateFilteredDict(field_mask, original_dict)
    self.assertDictEqual(expected_dict, merged_dict)

  def testCreateFilteredDictWithEmptyPath(self):
    """Tests a FieldMask with the empty string as a path."""
    original_dict = {'a': 1}

    field_mask = field_mask_pb2.FieldMask(paths=[''])

    with self.assertRaisesRegex(ValueError, 'Field cannot be empty string'):
      field_mask_util.CreateFilteredDict(field_mask, original_dict)

  def testCreateFilteredDictWithInvalidPath(self):
    """Tests a FieldMask with an invalid path."""
    original_dict = {'a': 1, 'c': 2}

    # Note that 'b' is not a path in the dict.
    field_mask = field_mask_pb2.FieldMask(paths=['a', 'b'])

    expected_dict = {'a': 1}

    with cros_test_lib.LoggingCapturer() as logging_capturer:
      merged_dict = field_mask_util.CreateFilteredDict(field_mask,
                                                       original_dict)

    # The merged dict should contain only field 'a' ('b' does not exist, 'c'
    # is not in the field mask). A warning about 'b' was logged.
    self.assertTrue(logging_capturer.LogsContain('Field b not found.'))
    self.assertDictEqual(expected_dict, merged_dict)

  def testCreateFilteredDictWithLists(self):
    """Tests selecting fields that are lists."""
    original_dict = {
        'a': [1, 2],
        'b': [3, 4],
        'c': {
            'd': [5, 6],
            'e': [7, 8],
        },
        'f': [{
            'g': 9,
            'h': 10,
        }, {
            'g': 11,
            'h': 12,
        }],
    }

    field_mask = field_mask_pb2.FieldMask(paths=['a', 'c.d', 'f'])

    expected_dict = {
        'a': [1, 2],
        'c': {
            'd': [5, 6],
        },
        'f': [{
            'g': 9,
            'h': 10,
        }, {
            'g': 11,
            'h': 12,
        }],
    }

    merged_dict = field_mask_util.CreateFilteredDict(field_mask, original_dict)
    self.assertDictEqual(expected_dict, merged_dict)

  def testCreateFilteredDictWithListSubFields(self):
    """Tests selecting a list that is not the last position in a path."""
    original_dict = {
        'a': [1, 2],
        'b': [3, 4],
        'c': {
            'd': [5, 6],
            'e': [7, 8],
        },
        'f': [{
            'g': 9,
            'h': 10,
        }, {
            'g': 11,
            'h': 12,
        }],
    }

    field_mask = field_mask_pb2.FieldMask(paths=['f.g'])

    with self.assertRaisesRegex(
        ValueError, 'Field f is a list and cannot have sub-fields'
    ):
      field_mask_util.CreateFilteredDict(field_mask, original_dict)


  def testCreateFilteredDictWithEmptyFieldMask(self):
    """Tests a FieldMask with no paths."""
    original_dict = {'a': 1}

    field_mask = field_mask_pb2.FieldMask()

    merged_dict = field_mask_util.CreateFilteredDict(field_mask, original_dict)
    self.assertDictEqual({}, merged_dict)
