#!/usr/bin/env python
#
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# gettoken.py can be used to get auth token from Gaia. It asks username and
# password and then prints token on the screen.

import urllib
import getpass
url = "https://www.google.com:443/accounts/ClientLogin"

print "Email:",
email = raw_input()

passwd = getpass.getpass("Password: ")

params = urllib.urlencode({'Email': email, 'Passwd': passwd,
                           'source': 'chromoting', 'service': 'chromiumsync',
                           'PersistentCookie': 'true', 'accountType': 'GOOGLE'})
f = urllib.urlopen(url, params);
print f.read()
