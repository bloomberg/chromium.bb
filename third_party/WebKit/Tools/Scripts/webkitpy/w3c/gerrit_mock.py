# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class MockGerritAPI(object):

    def __init__(self, host, user, token):
        self.host = host
        self.user = user
        self.token = token

    def query_exportable_open_cls(self):
        return []

    def get(self, path, raw=False):  # pylint: disable=unused-argument
        return '' if raw else {}

    def post(self, path, data):  # pylint: disable=unused-argument
        return {}
