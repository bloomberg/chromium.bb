#!/usr/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests to make sure we generate and update the expected-*.txt files
properly after running layout tests."""

import os
import sys
import unittest

import update_expectations_from_dashboard


class UpdateExpectationsUnittest(unittest.TestCase):
    ###########################################################################
    # Tests

    def testKeepsUnmodifiedLines(self):
        expectations = """// Ensure comments and newlines don't get stripped.
    BUG1 SLOW : 1.html = PASS

    BUG2 : 2.html = FAIL TIMEOUT
    """
        exp_results = """// Ensure comments and newlines don't get stripped.
    BUG1 SLOW : 1.html = PASS

    BUG2 : 2.html = FAIL TIMEOUT
    """

        updates = []
        self.updateExpectations(expectations, updates, exp_results)

    def testRemoveFlakyExpectation(self):
        expectations = "BUG1 : 1.html = TIMEOUT FAIL\n"
        expected_results = "BUG1 : 1.html = TIMEOUT\n"
        updates = {"1.html": {
            "WIN RELEASE": {"extra": "FAIL"},
            "WIN DEBUG": {"extra": "FAIL"},
            "LINUX RELEASE": {"extra": "FAIL"},
            "LINUX DEBUG": {"extra": "FAIL"},
            "MAC RELEASE": {"extra": "FAIL"},
            "MAC DEBUG": {"extra": "FAIL"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testRemoveExpectationSlowTest(self):
        expectations = "BUG1 SLOW : 1.html = FAIL\n"
        expected_results = "BUG1 SLOW : 1.html = PASS\n"
        updates = {"1.html": {
            "WIN RELEASE": {"extra": "FAIL"},
            "WIN DEBUG": {"extra": "FAIL"},
            "LINUX RELEASE": {"extra": "FAIL"},
            "LINUX DEBUG": {"extra": "FAIL"},
            "MAC RELEASE": {"extra": "FAIL"},
            "MAC DEBUG": {"extra": "FAIL"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testRemoveExpectation(self):
        expectations = "BUG1 : 1.html = FAIL\n"
        expected_results = ""
        updates = {"1.html": {
            "WIN RELEASE": {"extra": "FAIL"},
            "WIN DEBUG": {"extra": "FAIL"},
            "LINUX RELEASE": {"extra": "FAIL"},
            "LINUX DEBUG": {"extra": "FAIL"},
            "MAC RELEASE": {"extra": "FAIL"},
            "MAC DEBUG": {"extra": "FAIL"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testRemoveExpectationFromOnePlatform(self):
        expectations = "BUG1 : 1.html = FAIL\n"
        expected_results = """BUG1 MAC WIN DEBUG : 1.html = FAIL
    BUG1 RELEASE : 1.html = FAIL
    """
        updates = {"1.html": {"LINUX DEBUG": {"extra": "FAIL"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testRemoveSlow(self):
        expectations = "BUG1 SLOW : 1.html = PASS\n"
        expected_results = ""
        updates = {"1.html": {
            "WIN RELEASE": {"extra": "SLOW"},
            "WIN DEBUG": {"extra": "SLOW"},
            "LINUX RELEASE": {"extra": "SLOW"},
            "LINUX DEBUG": {"extra": "SLOW"},
            "MAC RELEASE": {"extra": "SLOW"},
            "MAC DEBUG": {"extra": "SLOW"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testAddFlakyExpectation(self):
        expectations = "BUG1 : 1.html = TIMEOUT\n"
        expected_results = "BUG1 : 1.html = TIMEOUT FAIL\n"
        updates = {"1.html": {
            "WIN RELEASE": {"missing": "FAIL"},
            "WIN DEBUG": {"missing": "FAIL"},
            "LINUX RELEASE": {"missing": "FAIL"},
            "LINUX DEBUG": {"missing": "FAIL"},
            "MAC RELEASE": {"missing": "FAIL"},
            "MAC DEBUG": {"missing": "FAIL"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testAddExpectationSlowTest(self):
        expectations = "BUG1 SLOW : 1.html = PASS\n"
        expected_results = "BUG1 SLOW : 1.html = PASS FAIL\n"
        updates = {"1.html": {
            "WIN RELEASE": {"missing": "FAIL"},
            "WIN DEBUG": {"missing": "FAIL"},
            "LINUX RELEASE": {"missing": "FAIL"},
            "LINUX DEBUG": {"missing": "FAIL"},
            "MAC RELEASE": {"missing": "FAIL"},
            "MAC DEBUG": {"missing": "FAIL"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testAddExpectation(self):
        # not yet implemented
        return

        expectations = ""
        expected_results = "BUG1 : 1.html = FAIL\n"
        updates = {"1.html": {
            "WIN RELEASE": {"missing": "FAIL"},
            "WIN DEBUG": {"missing": "FAIL"},
            "LINUX RELEASE": {"missing": "FAIL"},
            "LINUX DEBUG": {"missing": "FAIL"},
            "MAC RELEASE": {"missing": "FAIL"},
            "MAC DEBUG": {"missing": "FAIL"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testAddExpectationForOnePlatform(self):
        expectations = "BUG1 WIN : 1.html = TIMEOUT\n"
        expected_results = "BUG1 WIN : 1.html = TIMEOUT\n"
        # TODO(ojan): Once we add currently unlisted tests, this expect results
        # for this test should be:
        #expected_results = """BUG1 WIN : 1.html = TIMEOUT
        #BUG_AUTO LINUX DEBUG : 1.html = TIMEOUT
        #"""
        updates = {"1.html": {"LINUX DEBUG": {"missing": "TIMEOUT"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testAddSlow(self):
        expectations = "BUG1 : 1.html = FAIL\n"
        expected_results = "BUG1 SLOW : 1.html = FAIL\n"
        updates = {"1.html": {
            "WIN RELEASE": {"missing": "SLOW"},
            "WIN DEBUG": {"missing": "SLOW"},
            "LINUX RELEASE": {"missing": "SLOW"},
            "LINUX DEBUG": {"missing": "SLOW"},
            "MAC RELEASE": {"missing": "SLOW"},
            "MAC DEBUG": {"missing": "SLOW"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testAddRemoveMultipleExpectations(self):
        expectations = """BUG1 WIN : 1.html = FAIL
    BUG2 MAC : 1.html = FAIL"""
        expected_results = """BUG1 SLOW WIN : 1.html = FAIL
    BUG2 MAC : 1.html = TIMEOUT\n"""
        # TODO(ojan): Once we add currently unlisted tests, this expect results
        # for this test should be:
        #expected_results = """BUG1 SLOW WIN : 1.html = FAIL
        #BUG_AUTO LINUX SLOW : 1.html = PASS
        #BUG2 MAC : 1.html = TIMEOUT
        #"""

        updates = {"1.html": {
            "WIN RELEASE": {"missing": "SLOW"},
            "WIN DEBUG": {"missing": "SLOW"},
            "LINUX RELEASE": {"missing": "SLOW"},
            "LINUX DEBUG": {"missing": "SLOW"},
            "MAC RELEASE": {"missing": "TIMEOUT", "extra": "FAIL"},
            "MAC DEBUG": {"missing": "TIMEOUT", "extra": "FAIL"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testAddExistingExpectation(self):
        expectations = "BUG1 : 1.html = FAIL\n"
        expected_results = "BUG1 : 1.html = FAIL\n"
        updates = {"1.html": {"WIN RELEASE": {"missing": "FAIL"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testAddImageOrTextToFailExpectation(self):
        expectations = """BUG1 WIN RELEASE : 1.html = FAIL
    BUG1 MAC RELEASE : 1.html = FAIL
    BUG1 LINUX RELEASE : 1.html = FAIL
    BUG1 LINUX DEBUG : 1.html = TIMEOUT
    """
        expected_results = """BUG1 WIN RELEASE : 1.html = IMAGE+TEXT
    BUG1 MAC RELEASE : 1.html = IMAGE
    BUG1 LINUX RELEASE : 1.html = TEXT
    BUG1 LINUX DEBUG : 1.html = TIMEOUT IMAGE+TEXT
    """
        updates = {"1.html": {
            "WIN RELEASE": {"missing": "IMAGE+TEXT"},
            "MAC RELEASE": {"missing": "IMAGE"},
            "LINUX RELEASE": {"missing": "TEXT"},
            "LINUX DEBUG": {"missing": "IMAGE+TEXT"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testAddOther(self):
        # Other is a catchall for more obscure expectations results.
        # We should never add it to test_expectations.
        expectations = "BUG1 WIN RELEASE : 1.html = FAIL\n"
        expected_results = "BUG1 WIN RELEASE : 1.html = FAIL\n"
        updates = {"1.html": {"WIN RELEASE": {"missing": "OTHER"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testRemoveNonExistantExpectation(self):
        expectations = "BUG1 : 1.html = FAIL\n"
        expected_results = "BUG1 : 1.html = FAIL\n"
        updates = {"1.html": {"WIN RELEASE": {"extra": "TIMEOUT"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testUpdateSomePlatforms(self):
        expectations = "BUG1 DEBUG : 1.html = TEXT PASS\n"
        # TODO(ojan): Once we add currently unlisted tests, the expect results
        # for this test should include the missing bits for RELEASE.
        expected_results = "BUG1 LINUX DEBUG : 1.html = TEXT PASS\n"
        updates = {"1.html": {
            "WIN RELEASE": {"missing": "PASS TEXT"},
            "WIN DEBUG": {"extra": "MISSING TEXT"},
            "MAC RELEASE": {"missing": "PASS TEXT"},
            "MAC DEBUG": {"extra": "MISSING TEXT"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testAddTimeoutToSlowTest(self):
        # SLOW tests needing TIMEOUT need manual updating. Should just print
        # a log and not modify the test.
        expectations = "BUG1 SLOW : 1.html = TEXT\n"
        expected_results = "BUG1 SLOW : 1.html = TEXT\n"
        updates = {"1.html": {"WIN RELEASE": {"missing": "TIMEOUT"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testAddSlowToTimeoutTest(self):
        # SLOW tests needing TIMEOUT need manual updating. Should just print
        # a log and not modify the test.
        expectations = "BUG1 : 1.html = TIMEOUT\n"
        expected_results = "BUG1 : 1.html = TIMEOUT\n"
        updates = {"1.html": {"WIN RELEASE": {"missing": "SLOW"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testIncludeLastPlatformInFlakiness(self):
        # If a test is flaky on 5/6 platforms and the 6th's expectations are a
        # subset of the other 5/6, then give them all the same expectations.
        expectations = "BUG2 : 1.html = FAIL\n"
        expected_results = "BUG2 : 1.html = FAIL TIMEOUT\n"
        updates = {"1.html": {
            "WIN RELEASE": {"missing": "TIMEOUT", "extra": "FAIL"},
            "WIN DEBUG": {"missing": "TIMEOUT"},
            "LINUX RELEASE": {"missing": "TIMEOUT"},
            "LINUX DEBUG": {"missing": "TIMEOUT"},
            "MAC RELEASE": {"missing": "TIMEOUT"},
            "MAC DEBUG": {"missing": "TIMEOUT"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testIncludeLastPlatformInFlakinessThreeOutOfFour(self):
        # If a test is flaky on 5/6 platforms and the 6th's expectations are a
        # subset of the other 5/6, then give them all the same expectations.
        expectations = "BUG2 MAC LINUX : 1.html = FAIL\n"
        expected_results = "BUG2 LINUX MAC : 1.html = FAIL TIMEOUT\n"
        updates = {"1.html": {
            "LINUX RELEASE": {"missing": "TIMEOUT"},
            "MAC RELEASE": {"missing": "TIMEOUT"},
            "MAC DEBUG": {"missing": "TIMEOUT"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testExcludeLastPlatformFromFlakiness(self):
        # If a test is flaky on 5/6 platforms and the 6th's expectations
        # are not a subset of the other 5/6, then don't give them
        # all the same expectations.
        expectations = "BUG1 : 1.html = FAIL\n"
        expected_results = """BUG1 DEBUG : 1.html = FAIL TIMEOUT
    BUG1 LINUX MAC RELEASE : 1.html = FAIL TIMEOUT
    BUG1 WIN RELEASE : 1.html = FAIL CRASH
    """
        updates = {"1.html": {
            "WIN RELEASE": {"missing": "CRASH"},
            "WIN DEBUG": {"missing": "TIMEOUT"},
            "LINUX RELEASE": {"missing": "TIMEOUT"},
            "LINUX DEBUG": {"missing": "TIMEOUT"},
            "MAC RELEASE": {"missing": "TIMEOUT"},
            "MAC DEBUG": {"missing": "TIMEOUT"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testStripComments(self):
        expectations = """BUG1 : 1.html = TIMEOUT

    // Comment/whitespace should be removed when the test is.
    BUG2 WIN RELEASE : 2.html = TEXT

    // Comment/whitespace after test should remain.

    BUG2 MAC : 2.html = TEXT

    // Comment/whitespace at end of file should remain.
    """
        expected_results = """BUG1 : 1.html = TIMEOUT

    // Comment/whitespace after test should remain.

    BUG2 MAC DEBUG : 2.html = TEXT

    // Comment/whitespace at end of file should remain.
    """
        updates = {"2.html": {
            "WIN RELEASE": {"extra": "TEXT"},
            "MAC RELEASE": {"extra": "TEXT"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testLeaveComments(self):
        expectations = """BUG1 : 1.html = TIMEOUT

    // Comment/whitespace should remain.
    BUG2 : 2.html = FAIL PASS
    """
        expected_results = """BUG1 : 1.html = TIMEOUT

    // Comment/whitespace should remain.
    BUG2 MAC DEBUG : 2.html = FAIL PASS
    BUG2 LINUX MAC RELEASE : 2.html = FAIL PASS
    """
        updates = {"2.html": {
            "WIN RELEASE": {"extra": "FAIL"},
            "WIN DEBUG": {"extra": "FAIL"},
            "LINUX DEBUG": {"extra": "FAIL"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testLeaveCommentsIfNoWhitespaceAfterTest(self):
        expectations = """// Comment/whitespace should remain.
    BUG2 WIN RELEASE : 2.html = TEXT
    BUG2 : 1.html = IMAGE
    """
        expected_results = """// Comment/whitespace should remain.
    BUG2 : 1.html = IMAGE
    """
        updates = {"2.html": {"WIN RELEASE": {"extra": "TEXT"}}}
        self.updateExpectations(expectations, updates, expected_results)

    def testLeavesUnmodifiedExpectationsUntouched(self):
        # Ensures tests that would just change sort order of a line are noops.
        expectations = "BUG1 WIN LINUX : 1.html = TIMEOUT\n"
        expected_results = "BUG1 WIN LINUX : 1.html = TIMEOUT\n"
        updates = {"1.html": {"MAC RELEASE": {"missing": "SLOW"}}}
        self.updateExpectations(expectations, updates, expected_results)

    ###########################################################################
    # Helper functions

    def updateExpectations(self, expectations, updates, expected_results):
        results = update_expectations_from_dashboard.UpdateExpectations(
            expectations, updates)
        self.assertEqual(expected_results, results)

if '__main__' == __name__:
    unittest.main()
