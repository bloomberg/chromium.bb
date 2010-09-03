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

chromoting_auth_filepath = os.path.join(os.path.expanduser('~'),
                                        '.chromotingAuthToken')
chromoting_dir_auth_filepath = os.path.join(os.path.expanduser('~'),
                                            '.chromotingDirectoryAuthToken')

print "Email:",
email = raw_input()

passwd = getpass.getpass("Password: ")

chromoting_authenticator = gaia_auth.GaiaAuthenticator('chromiumsync');
chromoting_auth_token = chromoting_authenticator.authenticate(email, passwd)

chromoting_dir_authenticator = gaia_auth.GaiaAuthenticator('chromoting');
chromoting_dir_auth_token = \
    chromoting_dir_authenticator.authenticate(email, passwd)

# Set permission mask for created files.
os.umask(0066)

chromoting_auth_file = open(chromoting_auth_filepath, 'w')
chromoting_auth_file.write(email)
chromoting_auth_file.write('\n')
chromoting_auth_file.write(chromoting_auth_token)
chromoting_auth_file.close()

print
print 'Chromoting (sync) Auth Token:'
print
print chromoting_auth_token
print '...saved in', chromoting_auth_filepath

chromoting_dir_auth_file = open(chromoting_dir_auth_filepath, 'w')
chromoting_dir_auth_file.write(email)
chromoting_dir_auth_file.write('\n')
chromoting_dir_auth_file.write(chromoting_dir_auth_token)
chromoting_dir_auth_file.close()

print
print 'Chromoting Directory Auth Token:'
print
print chromoting_dir_auth_token
print '...saved in', chromoting_dir_auth_filepath
