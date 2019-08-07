# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""An interface to luci-auth.

The main usage is to get the OAuth access token for the service account on LUCI.
"""


class LuciAuth(object):

    def __init__(self, host):
        self._host = host

    def get_access_token(self):
        # ScriptError will be raised if luci-auth fails.
        output = self._host.executive.run_command(['luci-auth', 'token'])
        return output.strip()
