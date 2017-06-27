# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib


class MockChromiumCommit(object):

    def __init__(self, host,
                 position='refs/heads/master@{#123}',
                 message='Fake commit message',
                 patch='Fake patch contents',
                 change_id='Iba5eba11'):
        self.host = host
        self.position = position
        self.sha = hashlib.sha1(position).hexdigest()
        self._message = message
        self._patch = patch
        self._change_id = change_id

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

    def message(self):
        return self._message

    def subject(self):
        return self._message

    def format_patch(self):
        return self._patch

    def change_id(self):
        return self._change_id
