#!/usr/bin/env python
#
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Chromoting Directory API client implementation. Used for testing/debugging
# purposes. Requires Python 2.6: json module is not available in earlier
# versions.

import os
import httplib
import json
import urllib
import urllib2
import random
import sys

DEFAULT_DIRECTORY_SERVER = 'www.googleapis.com'

auth_filepath = os.path.join(os.path.expanduser('~'),
                             '.chromotingDirectoryAuthToken')

def random_uuid():
  return ("%04x%04x-%04x-%04x-%04x-%04x%04x%04x" %
    tuple(map(lambda x: random.randrange(0,65536), range(8))))

class Host:
  def __init__(self, parameters=None):
    if parameters != None:
      self.host_id = parameters[u"hostId"]
      self.host_name = parameters[u"hostName"]
      self.public_key = parameters[u"publicKey"]
      # Following fields may be missing, use get() for them.
      self.jabber_id = parameters.get(u"jabberId")
      self.created_time = parameters.get(u"createdTime")
      self.updated_time = parameters.get(u"updatedTime")
      self.status = parameters.get(u"status")
    else:
      self.host_id = random_uuid()
      import socket
      self.host_name = socket.gethostname()
      self.public_key = None
      self.jabber_id = None
      self.created_time = None
      self.updated_time = None
      self.status = None

class HostDirectoryError(Exception):
  def __init__(self, message, response):
    Exception.__init__(self, message)
    print response
    self._response = response

class HostDirectory:
  def __init__(self, username, auth_token, server=DEFAULT_DIRECTORY_SERVER):
    self._username = username
    self._auth_token = auth_token
    self._base_url = '/chromoting/v1/@me/hosts'

    self._http = httplib.HTTPSConnection(server)
    self._headers = {"Authorization": "GoogleLogin auth=" + self._auth_token,
                     "Content-Type": "application/json" }

  def add_host(self, host):
    host_json = { 'data':
                    { 'hostId': host.host_id,
                      'hostName': host.host_name,
                      'publicKey': host.public_key,
                      }
                  }
    if host.jabber_id:
      host_json['data']['jabberId'] = host.jabber_id
    post_data = json.dumps(host_json)
    self._http.request("POST", self._base_url, post_data, self._headers)
    response = self._http.getresponse()
    if response.status != 200:
      raise HostDirectoryError(response.reason, response.read())
    data = response.read()

  def get_hosts(self):
    self._http.request("GET", self._base_url, headers=self._headers)
    response = self._http.getresponse()
    if response.status != 200:
      raise HostDirectoryError(response.reason, response.read())
    data = response.read()
    data = json.loads(data)[u'data']
    results = []
    if data.has_key(u'items'):
      for item in data[u'items']:
        results.append(Host(item))
    return results

  def delete_host(self, host_id):
    url = self._base_url + '/' + host_id
    self._http.request("DELETE", url, headers=self._headers)
    response = self._http.getresponse()
    if response.status / 100 != 2: # Normally 204 is returned
      raise HostDirectoryError(response.reason, response.read())
    data = response.read()

def usage():
  sys.stderr.write(
      ("Usage:\n" +
       " Login: \t\t%(cmd)s login\n" +
       " Register host: \t%(cmd)s insert --hostId=<hostId>" +
       " --hostName=<hostName> \\\n" +
       "\t\t\t    --publicKey=<publicKey>  --jabberId=<jabberId>\n" +
       " List hosts: \t\t%(cmd)s list\n" +
       " Delete a host: \t%(cmd)s delete <host_id>\n")
                   % {"cmd" : sys.argv[0]})
  return 1

class CommandError(Exception):
  def __init__(self, message):
    Exception.__init__(self, message)

def load_auth_token():
  try:
    lines = open(auth_filepath).readlines()
  except IOError:
    raise CommandError(("Can't open file (%s). Please run " +
                        "'%s login' and try again.") %
                       (auth_filepath, sys.argv[0]))
  if len(lines) != 2:
    raise CommandError(("Invalid auth file (%s). Please run " +
                        "'%s login' and try again.") %
                       (auth_filepath, sys.argv[0]))
  return map(lambda x: x.strip(), lines)

def login_cmd(args):
  """login command"""
  if len(args) != 0:
    return usage()

  import getpass
  import gaia_auth

  print "Email:",
  email = raw_input()
  passwd = getpass.getpass("Password: ")

  authenticator = gaia_auth.GaiaAuthenticator('chromoting');
  auth_token = authenticator.authenticate(email, passwd)

  # Set permission mask for created file.
  os.umask(0066)
  auth_file = open(auth_filepath, 'w')
  auth_file.write(email)
  auth_file.write('\n')
  auth_file.write(auth_token)
  auth_file.close()

  print 'Auth token: ', auth_token
  print '...saved in', auth_filepath

def list_cmd(args):
  """list command"""
  if len(args) != 0:
    return usage()
  (username, token) = load_auth_token()
  client = HostDirectory(username, token)
  print '%36s  %30s   %s' % ("HOST ID", "HOST NAME", "JABBER ID")
  for host in client.get_hosts():
    print '%36s  %30s   %s' % (host.host_id, host.host_name, host.jabber_id)
  return 0

def insert_cmd(args):
  """insert command"""
  (username, token) = load_auth_token()
  client = HostDirectory(username, token)

  host = Host()
  for arg in args:
    if arg.startswith("--hostId="):
      host.host_id = arg[len("--hostId="):]
    elif arg.startswith("--hostName="):
      host.host_name = arg[len("--hostName="):]
    elif arg.startswith("--publicKey="):
      host.public_key = arg[len("--publicKey="):]
    elif arg.startswith("--jabberId="):
      host.jabber_id = arg[len("--jabberId="):]
    else:
      return usage()

  client.add_host(host)
  return 0

def delete_cmd(args):
  """delete command"""
  if len(args) != 1:
    return usage()
  host_id = args[0]
  (username, token) = load_auth_token()
  client = HostDirectory(username, token)
  client.delete_host(host_id)
  return 0

def main():
  import sys
  args = sys.argv[1:]
  if len(args) == 0:
    return usage()
  command = args[0]

  try:
    if command == "help":
      usage()
    elif command == "login":
      return login_cmd(args[1:])
    elif command == "list":
      return list_cmd(args[1:])
    elif command == "insert":
      return insert_cmd(args[1:])
    elif command == "delete":
      return delete_cmd(args[1:])
    else:
      raise CommandError("Unknown command: %s" % command);

  except CommandError as e:
    sys.stderr.write("%s\n" % e.args[0])
    return 1

  return 0

if __name__ == '__main__':
  sys.exit(main())
