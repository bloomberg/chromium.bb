#!/usr/bin/env python
#
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# runclient.py gets the chromoting host info from an input arg and then
# tries to find the authentication info in the .chromotingAuthToken file
# so that the host authentication arguments can be automatically set.

import os

auth_filename = '.chromotingAuthToken'

# Read username and auth token from token file.
auth = open(auth_filename)
authinfo = auth.readlines()

username = authinfo[0].rstrip()
authtoken = authinfo[1].rstrip()

# Request final 8 characters of Host JID from user.
# This assumes that the host is published under the same username as the
# client attempting to connect.
print 'Host JID:', username + '/chromoting',
hostjid_suffix = raw_input()
hostjid = username + '/chromoting' + hostjid_suffix

# TODO(garykac): Make this work on Windows.
command = []
command.append('../../out/Debug/chromoting_x11_client')
command.append('--host_jid ' + hostjid)
command.append('--jid ' + username)
command.append('--token ' + authtoken)

# Launch the client
os.system(' '.join(command))
