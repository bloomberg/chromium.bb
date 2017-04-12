# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class MockGerrit(object):

    def __init__(self, host, user, token):
        self.host = host
        self.user = user
        self.token = token

    def query_open_cls(self):
        return []
