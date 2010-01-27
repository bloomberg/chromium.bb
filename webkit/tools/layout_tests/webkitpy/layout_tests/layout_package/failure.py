# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CHROMIUM_WIN = "chromium-win"
CHROMIUM_MAC = "chromium-mac"
CHROMIUM_LINUX = "chromium-linux"
WEBKIT_WIN_TITLE = "WebKit Win"
WEBKIT_MAC_TITLE = "WebKit Mac"
WEBKIT_TITLE = "WebKit"
UNKNOWN = "Unknown"

EXPECTED_IMAGE_FILE_ENDING = "-expected.png"
ACTUAL_IMAGE_FILE_ENDING = "-actual.png"
UPSTREAM_IMAGE_FILE_ENDING = "-expected-upstream.png"
EXPECTED_TEXT_FILE_ENDING = "-expected.txt"
ACTUAL_TEXT_FILE_ENDING = "-actual.txt"
DIFF_IMAGE_FILE_ENDING = "-diff.png"
DIFF_TEXT_FILE_ENDING = "-diff.txt"

CHROMIUM_SRC_HOME = "http://src.chromium.org/viewvc/chrome/trunk/src/webkit/"
CHROMIUM_TRAC_HOME = CHROMIUM_SRC_HOME + "data/layout_tests/"
WEBKIT_TRAC_HOME = "http://trac.webkit.org/browser/trunk/LayoutTests/"
WEBKIT_SVN_HOSTNAME = "svn.webkit.org"
THIRD_PARTY = "third_party"

WEBKIT_PLATFORM_URL_BASE = WEBKIT_TRAC_HOME + "platform"
WEBKIT_LAYOUT_TEST_BASE_URL = "http://svn.webkit.org/repository/webkit/trunk/"
WEBKIT_IMAGE_BASELINE_BASE_URL_WIN = (WEBKIT_LAYOUT_TEST_BASE_URL +
                                      "LayoutTests/platform/win/")
WEBKIT_IMAGE_BASELINE_BASE_URL_MAC = (WEBKIT_LAYOUT_TEST_BASE_URL +
                                      "LayoutTests/platform/mac/")
WEBKIT_TRAC_IMAGE_BASELINE_BASE_URL_MAC = WEBKIT_PLATFORM_URL_BASE + "/mac/"
WEBKIT_TRAC_IMAGE_BASELINE_BASE_URL_WIN = WEBKIT_PLATFORM_URL_BASE + "/win/"

LAYOUT_TEST_RESULTS_DIR = "layout-test-results"

FAIL = "FAIL"
TIMEOUT = "TIMEOUT"
CRASH = "CRASH"
PASS = "PASS"
WONTFIX = "WONTFIX"


class Failure(object):
    """
    This class represents a failure in the test output, and is
    intended as a data model object.
    """

    def __init__(self):
        self.platform = ""
        self.test_path = ""
        self.text_diff_mismatch = False
        self.image_mismatch = False
        self.timeout = False
        self.crashed = False
        self.text_baseline_url = ""
        self.image_baseline_url = ""
        self.text_baseline_age = ""
        self.image_baseline_age = ""
        self.test_age = ""
        self.text_baseline_local = ""
        self.image_baseline_local = ""
        self.text_actual_local = ""
        self.image_actual_local = ""
        self.image_baseline_upstream_url = ""
        self.image_baseline_upstream_local = ""
        self.test_expectations_line = ""
        self.flakiness = 0

    def get_expected_image_filename(self):
        return self._rename_end_of_test_path(EXPECTED_IMAGE_FILE_ENDING)

    def get_actual_image_filename(self):
        return self._rename_end_of_test_path(ACTUAL_IMAGE_FILE_ENDING)

    def get_expected_text_filename(self):
        return self._rename_end_of_test_path(EXPECTED_TEXT_FILE_ENDING)

    def get_actual_text_filename(self):
        return self._rename_end_of_test_path(ACTUAL_TEXT_FILE_ENDING)

    def get_image_diff_filename(self):
        return self._rename_end_of_test_path(DIFF_IMAGE_FILE_ENDING)

    def get_text_diff_filename(self):
        return self._rename_end_of_test_path(DIFF_TEXT_FILE_ENDING)

    def get_image_upstream_filename(self):
        return self._rename_end_of_test_path(UPSTREAM_IMAGE_FILE_ENDING)

    def _rename_end_of_test_path(self, suffix):
        last_index = self.test_path.rfind(".")
        if last_index == -1:
            return self.test_path
        return self.test_path[0:last_index] + suffix

    def get_test_home(self):
        if self.test_path.startswith("chrome"):
            return CHROMIUM_TRAC_HOME + self.test_path
        return WEBKIT_TRAC_HOME + self.test_path

    def get_image_baseline_trac_home(self):
        if self.is_image_baseline_in_webkit():
            return self._get_trac_home(self.image_baseline_url)
        return self.image_baseline_url

    def get_text_baseline_trac_home(self):
        if self.text_baseline_url and self.is_text_baseline_in_webkit():
            return self._get_trac_home(self.text_baseline_url)
        return self.text_baseline_url

    def _get_trac_home(self, file):
        return WEBKIT_TRAC_HOME + file[file.find("LayoutTests"):]

    def get_text_baseline_location(self):
        return self._get_file_location(self.text_baseline_url)

    def get_image_baseline_location(self):
        return self._get_file_location(self.image_baseline_url)

    # TODO(gwilson): Refactor this logic so it can be used by multiple scripts.
    # TODO(gwilson): Change this so that it respects the fallback order of
    # different platforms. (If platform is mac, the fallback should be
    # different.)

    def _get_file_location(self, file):
        if not file:
            return None
        if file.find(CHROMIUM_WIN) > -1:
            return CHROMIUM_WIN
        if file.find(CHROMIUM_MAC) > -1:
            return CHROMIUM_MAC
        if file.find(CHROMIUM_LINUX) > -1:
            return CHROMIUM_LINUX
        if file.startswith(WEBKIT_IMAGE_BASELINE_BASE_URL_WIN):
            return WEBKIT_WIN_TITLE
        if file.startswith(WEBKIT_IMAGE_BASELINE_BASE_URL_MAC):
            return WEBKIT_MAC_TITLE
        # TODO(gwilson): Add mac-snowleopard, mac-leopard, mac-tiger here.
        if file.startswith(WEBKIT_LAYOUT_TEST_BASE_URL):
            return WEBKIT_TITLE
        return UNKNOWN

    def _is_file_in_webkit(self, file):
        return file != None and (file.find(WEBKIT_SVN_HOSTNAME) > -1 or
                                 file.find(THIRD_PARTY) > -1)

    def is_image_baseline_in_chromium(self):
        return not self.is_image_baseline_in_webkit()

    def is_image_baseline_in_webkit(self):
        return self._is_file_in_webkit(self.image_baseline_url)

    def is_text_baseline_in_chromium(self):
        return not self.is_text_baseline_in_webkit()

    def is_text_baseline_in_webkit(self):
        return self._is_file_in_webkit(self.text_baseline_url)

    def get_text_result_location_in_zip_file(self):
        return self._get_file_location_in_zip_file(
            self.get_actual_text_filename())

    def get_image_result_location_in_zip_file(self):
        return self._get_file_location_in_zip_file(
            self.get_actual_image_filename())

    def _get_file_location_in_zip_file(self, file):
        return "%s/%s" % (LAYOUT_TEST_RESULTS_DIR, file)

    # TODO(gwilson): implement this method.
    def get_all_baseline_locations(self):
        return None

    # This method determines whether the test is actually expected to fail,
    # in order to know whether to retrieve expected test results for it.
    # (test results dont exist for tests expected to fail/crash.)

    def is_expected_to_fail(self):
        return self._find_keyword_in_expectations(FAIL)

    def is_expected_to_timeout(self):
        return self._find_keyword_in_expectations(TIMEOUT)

    def is_expected_to_crash(self):
        return self._find_keyword_in_expectations(CRASH)

    def is_expected_to_pass(self):
        return self._find_keyword_in_expectations(PASS)

    def is_wont_fix(self):
        return self._find_keyword_in_expectations(WONTFIX)

    def _find_keyword_in_expectations(self, keyword):
        if (not self.test_expectations_line or
            len(self.test_expectations_line) == 0):
            return False
        if self.test_expectations_line.find(keyword) > -1:
            return True
        return False
