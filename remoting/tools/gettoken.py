#!/usr/bin/env python
#
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# gettoken.py can be used to get auth token from Gaia. It asks username and
# password and then prints token on the screen.

import getpass
import os
import urllib

import gaia_auth

auth_filepath = os.path.join(os.path.expanduser('~'), '.chromotingAuthToken')

print "Email:",
email = raw_input()

passwd = getpass.getpass("Password: ")

authenticator = gaia_auth.GaiaAuthenticator('chromiumsync');
auth_token = authenticator.authenticate(email, passwd)

# Set permission mask for created file.
os.umask(0066)
auth_file = open(auth_filepath, 'w')
auth_file.write(email)
auth_file.write('\n')
auth_file.write(auth_token)
auth_file.close()

print
print 'Auth token:'
print
print auth_token
print '...saved in', auth_filepath
