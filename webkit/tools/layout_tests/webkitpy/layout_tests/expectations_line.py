#!/usr/bin/env python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import os
import re
import sys

#
# Find the WebKit python directories and add them to the PYTHONPATH
#
try:
    f = __file__
except NameError:
    f = sys.argv[0]

this_file = os.path.abspath(f)
base_dir = this_file[0:this_file.find('webkit'+ os.sep + 'tools')]
webkitpy_dir = os.path.join(base_dir, 'third_party', 'WebKit', 'WebKitTools',
                            'Scripts')
sys.path.append(webkitpy_dir)

from webkitpy.layout_tests.layout_package import test_expectations


class ExpectationsLine(test_expectations.TestExpectationsFile):
    """Represents a single line from test_expectations.txt. It is instantiated
    like so:

    ExpectationsLine("BUG1 LINUX : fast/dom/test.html = TEXT")
    """
    builds = set(['release', 'debug'])
    platforms = set(['linux', 'win', 'mac'])
    attributes = set(['^bug', 'slow', 'wontfix', 'skip', 'defer'])

    def __init__(self, line):
        self._line = line
        self._test, self._options, self._expectations = (
            self.parse_expectations_line(line, 0))

        self._options = set(self._options)

        self._attributes = set(self._remove_from_options(ExpectationsLine.attributes))
        self._platforms = set(self._remove_from_options(ExpectationsLine.platforms))
        self._builds = set(self._remove_from_options(ExpectationsLine.builds))

        if len(self._platforms) == 0:
            self._platforms = ExpectationsLine.platforms

        if len(self._builds) == 0:
            self._builds = ExpectationsLine.builds

    def _remove_from_options(self, regexes):
        result = []
        for option in self._options:
            for regex in regexes:
                if re.match(regex, option) and not (option in result):
                    result.append(option)

        for removed_option in result:
            self._options.remove(removed_option)
        return result

    def can_merge(self, other):
        if self._test != other._test:
            return False
        if self._expectations != other._expectations:
            return False
        if self._attributes != other._attributes:
            return False

        self2 = copy.deepcopy(self)

        expected_len = self2.target_count() + other.target_count()

        self2.merge(other)

        return self2.target_count() == expected_len

    def merge(self, other):
        for build in other._builds:
            if build not in self._builds:
                self._builds.add(build)
        for platform in other._platforms:
            if platform not in self._platforms:
                self._platforms.add(platform)

    def does_target(self, platform, build):
        return (self._empty_or_contains(platform, self._platforms) and
                self._empty_or_contains(build, self._builds))

    def _empty_or_contains(self, elem, list):
        return len(list) == 0 or elem in list

    def target_count(self):
        return len(self._platforms) * len(self._builds)

    def targets(self):
        result = []
        for platform in ExpectationsLine.platforms:
            for build in ExpectationsLine.builds:
                if self.does_target(platform, build):
                    result += [(platform, build)]
        return result

    def can_add_target(self, platform, build):
        platform_count = len(self._platforms)
        if not platform in self._platforms:
            platform_count += 1

        build_count = len(self._builds)
        if not build in self._builds:
            build_count += 1

        targets_after_add = platform_count * build_count
        return targets_after_add == self.target_count() + 1

    def add_target(self, platform, build):
        if not platform in self._platforms:
            self._platforms.append(platform)
        if not build in self._builds:
            self._builds.append(build)

    def _omit_if_all_present(self, values, possible_values):
        if possible_values == values:
            return set()
        return values

    def _sorted_list(self, values):
        result = list(values)
        result.sort()
        return result

    def __str__(self):
        builds_and_platforms = []
        builds_and_platforms += self._sorted_list(self._omit_if_all_present(self._platforms, ExpectationsLine.platforms))
        builds_and_platforms += self._sorted_list(self._omit_if_all_present(self._builds, ExpectationsLine.builds))

        result =  '%s : %s = %s' % (
            " ".join(self._sorted_list(self._attributes) + builds_and_platforms + self._sorted_list(self._options)).upper(),
            self._test,
            " ".join(self._expectations).upper())
        return result

