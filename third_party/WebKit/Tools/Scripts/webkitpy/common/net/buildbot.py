# Copyright (c) 2009, Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import urllib2

import webkitpy.common.config.urls as config_urls
from webkitpy.common.memoized import memoized
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.net.networktransaction import NetworkTransaction
from webkitpy.common.system.logutils import get_logger


_log = get_logger(__file__)


class Builder(object):

    def __init__(self, builder_name, buildbot):
        self._name = builder_name
        self._buildbot = buildbot

    def name(self):
        return self._name

    def results_url(self):
        return config_urls.chromium_results_url_base_for_builder(self._name)

    def latest_layout_test_results_url(self):
        return config_urls.chromium_accumulated_results_url_base_for_builder(self._name)

    @memoized
    def latest_layout_test_results(self):
        return self.fetch_layout_test_results(self.latest_layout_test_results_url())

    def _fetch_file_from_results(self, results_url, file_name):
        # It seems this can return None if the url redirects and then returns 404.
        result = urllib2.urlopen("%s/%s" % (results_url, file_name))
        if not result:
            return None
        # urlopen returns a file-like object which sometimes works fine with str()
        # but sometimes is a addinfourl object.  In either case calling read() is correct.
        return result.read()

    def fetch_layout_test_results(self, results_url):
        # FIXME: This should cache that the result was a 404 and stop hitting the network.
        results_file = NetworkTransaction(convert_404_to_None=True).run(
            lambda: self._fetch_file_from_results(results_url, "failing_results.json"))
        revision = NetworkTransaction(convert_404_to_None=True).run(
            lambda: self._fetch_file_from_results(results_url, "LAST_CHANGE"))
        if not revision:
            results_file = None
        return LayoutTestResults.results_from_string(results_file, revision)

    def build(self, build_number):
        return Build(self, build_number=build_number)


class Build(object):

    def __init__(self, builder, build_number):
        self._builder = builder
        self._number = build_number

    def results_url(self):
        return "%s/%s/layout-test-results" % (self._builder.results_url(), self._number)

    def builder(self):
        return self._builder


class BuildBot(object):

    def builder_with_name(self, builder_name):
        return Builder(builder_name, self)
