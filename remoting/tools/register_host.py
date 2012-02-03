#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Registers new hosts in chromoting directory.

It asks for username/password and then writes these settings to config file.
"""

import base64
import getpass
import hashlib
import hmac
import json
import os
import random
import socket
import sys
import urllib
import urllib2

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

  while 1:
    pin = getpass.getpass("Host PIN: ")
    if len(pin) < 4:
      print "PIN must be at least 4 characters long."
      continue
    pin2 = getpass.getpass("Confirm host PIN: ")
    if pin2 != pin:
      print "PINs didn't match. Please try again."
      continue
    break
  host_secret_hash = "hmac:" + base64.b64encode(
      hmac.new(str(host_id), pin, hashlib.sha256).digest())

  params = { "data": {
      "hostId": host_id,
      "hostName": host_name,
      "publicKey": public_key,
      } }
  headers = {"Authorization": "GoogleLogin auth=" + auth_token,
             "Content-Type": "application/json" }
  request = urllib2.Request(url, json.dumps(params), headers)

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
  config = {
      "xmpp_login" : email,
      "xmpp_auth_token" : auth_token,
      "host_id" : host_id,
      "host_name" : host_name,
      "host_secret_hash": host_secret_hash,
      "private_key" : private_key,
      }

  settings_file.write(json.dumps(config, indent=2))
  settings_file.close()

  print 'Configuration saved in', settings_filepath
  return 0


if __name__ == '__main__':
  sys.exit(main())
