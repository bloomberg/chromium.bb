# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from SimpleHTTPServer import SimpleHTTPRequestHandler
import cgi
import json
import os
import shutil
import SimpleHTTPServer
import socket
import SocketServer
import sys
import threading

# change into script in order to serve tpview files
script_dir = os.path.dirname(os.path.realpath(__file__))
os.chdir(script_dir)

class ServerData(object):
  def __init__(self, log):
    self.log = log
    self.loaded = False
    self.saved = False
    self.result = None

class TPViewHTTPRequestHandler(SimpleHTTPRequestHandler):
  """
    serves static files and provides three dynamic URLs:
    /edit serving view.html
    /load/* serving the log file provided when starting the server
    /save/* for POST'ing trimmed log files

    The latter two are only to be used with AJAX commands from
    /edit
  """
  def respond(self, data, type="text/html"):
      # send text response to browser
      self.send_response(200)
      self.send_header("Content-type", type)
      self.end_headers()
      self.wfile.write(data)

  def do_GET(self):
    data = self.server.user_data

    if self.path.startswith("/load/"):
      self.respond(data.log, "text/plain")
      data.loaded = True

    elif self.path.startswith("/edit"):
      self.path = "view.html"
      SimpleHTTPRequestHandler.do_GET(self)

    else:
      SimpleHTTPRequestHandler.do_GET(self)

  def do_POST(self):
    data = self.server.user_data

    if self.path.startswith("/save/"):
      name = os.path.basename(self.path)
      length = int(self.headers.getheader('content-length'))
      data.result = self.rfile.read(length)
      data.saved = True
      self.respond("Success")


def View(port, log=None, persistent=False):
  """
    Serve TPView viewing 'log'. The server will exit after serving TPView
    unless persistent is set to True.
  """
  data = ServerData(log)
  httpd = SocketServer.TCPServer(("", port), TPViewHTTPRequestHandler)
  httpd.user_data = data

  while True:
    httpd.handle_request()
    if not persistent and data.loaded:
      return


def Edit(port, log=None):
  """
    Serve TPView for editing 'log'. Blocks until the file has been trimmed
    and returns the trimmed log.
  """
  data = ServerData(log)
  httpd = SocketServer.TCPServer(("", port), TPViewHTTPRequestHandler)
  httpd.user_data = data

  while True:
    httpd.handle_request()
    if data.saved:
      return data.result


def Serve(port):
  """
    Serve TPView without any log data. The server will serve until killed
    externally (e.g. via keyboard interrupt).
  """
  data = ServerData("")
  httpd = SocketServer.TCPServer(("", port), TPViewHTTPRequestHandler)
  httpd.user_data = data
  while True:
    httpd.handle_request()
