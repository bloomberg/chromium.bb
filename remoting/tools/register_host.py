#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Registers new hosts in chromoting directory.

It asks for username/password and then writes these settings to config file.
"""

import getpass
import os
import urllib
import urllib2
import random
import socket
import sys

import gaia_auth
import keygen

def random_uuid():
  return ("%04x%04x-%04x-%04x-%04x-%04x%04x%04x" %
    tuple(map(lambda x: random.randrange(0,65536), range(8))))


def main():
  server = 'www.googleapis.com'
  url = 'https://' + server + '/chromoting/v1/@me/hosts'

  settings_filepath = os.path.join(os.path.expanduser('~'),
                                  '.ChromotingConfig.json')

  print "Email:",
  email = raw_input()
  password = getpass.getpass("Password: ")

  chromoting_auth = gaia_auth.GaiaAuthenticator('chromoting')
  auth_token = chromoting_auth.authenticate(email, password)

  host_id = random_uuid()
  print "HostId:", host_id
  host_name = socket.gethostname()
  print "HostName:", host_name

  print "Generating RSA key pair...",
  (private_key, public_key) = keygen.generateRSAKeyPair()
  print "Done"

  params = ('{"data":{' +
            '"hostId": "%(hostId)s",' +
            '"hostName": "%(hostName)s",' +
            '"publicKey": "%(publicKey)s"}}') %
            {'hostId': host_id, 'hostName': host_name,
            'publicKey': public_key}
  headers = {"Authorization": "GoogleLogin auth=" + auth_token,
            "Content-Type": "application/json" }
  request = urllib2.Request(url, params, headers)

  opener = urllib2.OpenerDirector()
  opener.add_handler(urllib2.HTTPDefaultErrorHandler())

  print
  print "Registering host with directory service..."
  try:
    res = urllib2.urlopen(request)
    data = res.read()
  except urllib2.HTTPError, err:
    print >> sys.stderr, "Directory returned error:", err
    print >> sys.stderr, err.fp.read()
    return 1

  print "Done"

  # Get token that the host will use to athenticate in talk network.
  authenticator = gaia_auth.GaiaAuthenticator('chromiumsync');
  auth_token = authenticator.authenticate(email, password)

  # Write settings file.
  os.umask(0066) # Set permission mask for created file.
  settings_file = open(settings_filepath, 'w')
  settings_file.write('{\n');
  settings_file.write('  "xmpp_login" : "' + email + '",\n')
  settings_file.write('  "xmpp_auth_token" : "' + auth_token + '",\n')
  settings_file.write('  "host_id" : "' + host_id + '",\n')
  settings_file.write('  "host_name" : "' + host_name + '",\n')
  settings_file.write('  "private_key" : "' + private_key + '",\n')
  settings_file.write('}\n')
  settings_file.close()

  print 'Configuration saved in', settings_filepath
  return 0


if __name__ == '__main__':
  sys.exit(main())
