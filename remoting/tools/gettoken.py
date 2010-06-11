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

url = "https://www.google.com:443/accounts/ClientLogin"
auth_filename = '.chromotingAuthToken'

print "Email:",
email = raw_input()

passwd = getpass.getpass("Password: ")

params = urllib.urlencode({'Email': email, 'Passwd': passwd,
                           'source': 'chromoting', 'service': 'chromiumsync',
                           'PersistentCookie': 'true', 'accountType': 'GOOGLE'})
f = urllib.urlopen(url, params);

auth_found = False
for line in f:
  if line.startswith('Auth='):
    auth_string = line[5:]

    # Set permission mask for created file.
    os.umask(0066)
    auth_file = open(auth_filename, 'w')
    auth_file.write(email)
    auth_file.write('\n')
    auth_file.write(auth_string)
    auth_file.close()

    print
    print 'Auth token:'
    print
    print auth_string
    print '...saved in', auth_filename
    auth_found = True

if not auth_found:
  print 'ERROR - Unable to find Auth token in output:'
  for line in f:
    print line,
