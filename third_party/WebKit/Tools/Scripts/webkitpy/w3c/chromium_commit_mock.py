# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib


class MockChromiumCommit(object):

    def __init__(self, host, position='refs/heads/master@{#123}'):
        self.host = host
        self.position = position
        self.sha = hashlib.sha1(position).hexdigest()

    @property
    def short_sha(self):
        return self.sha[0:10]

    def filtered_changed_files(self):
        return [
            'third_party/WebKit/LayoutTests/external/wpt/one.html',
            'third_party/WebKit/LayoutTests/external/wpt/two.html',
        ]

    def url(self):
        return 'https://fake-chromium-commit-viewer.org/+/%s' % self.short_sha
