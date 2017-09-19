# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import unittest

import generate_expired_histograms_array

_EXPECTED_HEADER_FILE_CONTENT = (
"""// Generated from generate_expired_histograms_array.py. Do not edit!

#ifndef TEST_TEST_H_
#define TEST_TEST_H_

#include <stdint.h>

namespace some_namespace {{

// Contains hashes of expired histograms.
{array_definition}

}}  // namespace some_namespace

#endif  // TEST_TEST_H_
""")

_EXPECTED_NON_EMPTY_ARRAY_DEFINITION = (
"""const uint64_t kExpiredHistogramsHashes[] = {
  0x965ce8e9e12a9c89,  // Test.FirstHistogram
  0xdb5b2f55ffd139e8,  // Test.SecondHistogram
};

const size_t kNumExpiredHistograms = 2;"""
)

_EXPECTED_EMPTY_ARRAY_DEFINITION = (
"""const uint64_t kExpiredHistogramsHashes[] = {
  0x0000000000000000,  // Dummy.Histogram
};

const size_t kNumExpiredHistograms = 1;"""
)

class ExpiredHistogramsTest(unittest.TestCase):

  def testGetExpiredHistograms(self):
    histograms = {
        "FirstHistogram": {
            "expiry_date": "2000/10/01"
        },
        "SecondHistogram": {
            "expiry_date": "2002/10/01"
        },
        "ThirdHistogram": {
            "expiry_date": "2001/10/01"
        },
        "FourthHistogram": {},
        "FifthHistogram": {
            "obsolete": "Has expired.",
            "expiry_date": "2000/10/01"
        }
    }

    base_date = datetime.date(2001, 10, 1)

    expired_histograms_names = (
        generate_expired_histograms_array._GetExpiredHistograms(
            histograms, base_date))

    self.assertEqual(expired_histograms_names, ["FirstHistogram"])

  def testBadExpiryDate(self):
    histograms = {
        "FirstHistogram": {
            "expiry_date": "2000/10/01"
        },
        "SecondHistogram": {
            "expiry_date": "2000-10-01"
        },
    }
    base_date = datetime.date(2000, 10, 01)

    with self.assertRaises(generate_expired_histograms_array.Error) as error:
        generate_expired_histograms_array._GetExpiredHistograms(histograms,
            base_date)

    self.assertEqual(
        "Unable to parse expiry date 2000-10-01 in histogram SecondHistogram.",
        str(error.exception))

  def testGetBaseDate(self):
    pattern = generate_expired_histograms_array._DATE_FILE_PATTERN

    # Does not match the pattern.
    content = "MAJOR_BRANCH__FAKE_DATE=2017/09/09"
    with self.assertRaises(generate_expired_histograms_array.Error):
        generate_expired_histograms_array._GetBaseDate(content, pattern)

    # Has invalid format.
    content = "MAJOR_BRANCH_DATE=2010-01-01"
    with self.assertRaises(generate_expired_histograms_array.Error):
        generate_expired_histograms_array._GetBaseDate(content, pattern)

    # Has invalid format.
    content = "MAJOR_BRANCH_DATE=2010/20/02"
    with self.assertRaises(generate_expired_histograms_array.Error):
        generate_expired_histograms_array._GetBaseDate(content, pattern)

    # Has invalid date.
    content = "MAJOR_BRANCH_DATE=2017/02/29"
    with self.assertRaises(generate_expired_histograms_array.Error):
        generate_expired_histograms_array._GetBaseDate(content, pattern)

    content = "!!FOO!\nMAJOR_BRANCH_DATE=2010/01/01\n!FOO!!"
    base_date = generate_expired_histograms_array._GetBaseDate(content, pattern)
    self.assertEqual(base_date, datetime.date(2010, 01, 01))

  def testGenerateHeaderFileContent(self):
    header_filename = "test/test.h"
    namespace = "some_namespace"

    histogram_map = generate_expired_histograms_array._GetHashToNameMap(
        ["Test.FirstHistogram", "Test.SecondHistogram"])
    expected_histogram_map = {
        "0x965ce8e9e12a9c89": "Test.FirstHistogram",
        "0xdb5b2f55ffd139e8": "Test.SecondHistogram",
    }
    self.assertEqual(expected_histogram_map, histogram_map)

    content = generate_expired_histograms_array._GenerateHeaderFileContent(
        header_filename, namespace, histogram_map)

    self.assertEqual(_EXPECTED_HEADER_FILE_CONTENT.format(
        array_definition=_EXPECTED_NON_EMPTY_ARRAY_DEFINITION), content)

  def testGenerateHeaderFileContentEmptyArray(self):
    header_filename = "test/test.h"
    namespace = "some_namespace"
    content = generate_expired_histograms_array._GenerateHeaderFileContent(
        header_filename, namespace, dict())
    self.assertEqual(_EXPECTED_HEADER_FILE_CONTENT.format(
        array_definition=_EXPECTED_EMPTY_ARRAY_DEFINITION), content)


if __name__ == "__main__":
  unittest.main()
