# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple Markdown browser for a Git checkout."""
from __future__ import print_function

import SimpleHTTPServer
import SocketServer
import argparse
import codecs
import os
import socket
import sys


THIS_DIR = os.path.abspath(os.path.dirname(__file__))
SRC_DIR = os.path.dirname(os.path.dirname(THIS_DIR))
sys.path.append(os.path.join(SRC_DIR, 'third_party', 'Python-Markdown'))
import markdown


def main(argv):
  parser = argparse.ArgumentParser(prog='md_browser')
  parser.add_argument('-p', '--port', type=int, default=8080,
                      help='port to run on (default = %(default)s)')
  args = parser.parse_args(argv)

  try:
    s = Server(args.port, SRC_DIR)
    print("Listening on http://localhost:%s/" % args.port)
    s.serve_forever()
    s.shutdown()
    return 0
  except KeyboardInterrupt:
    return 130


class Server(SocketServer.TCPServer):
  def __init__(self, port, top_level):
    SocketServer.TCPServer.__init__(self, ('0.0.0.0', port), Handler)
    self.port = port
    self.top_level = top_level

  def server_bind(self):
    self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    self.socket.bind(self.server_address)


class Handler(SimpleHTTPServer.SimpleHTTPRequestHandler):
  def do_GET(self):
    path = self.path

    # strip off the repo and branch info, if present, for compatibility
    # with gitiles.
    if path.startswith('/chromium/src/+/master'):
      path = path[len('/chromium/src/+/master'):]

    full_path = os.path.abspath(os.path.join(self.server.top_level, path[1:]))

    if not full_path.startswith(SRC_DIR):
      self._DoUnknown()
    elif path == '/doc.css':
      self._WriteTemplate('doc.css')
    elif not os.path.exists(full_path):
      self._DoNotFound()
    elif path.lower().endswith('.md'):
      self._DoMD(path)
    else:
      self._DoUnknown()

  def _DoMD(self, path):
    extensions = [
        'markdown.extensions.fenced_code',
        'markdown.extensions.tables',
        'markdown.extensions.toc',
        'gitiles_ext_blocks',
    ]

    contents = self._Read(path[1:])
    md_fragment = markdown.markdown(contents,
                                    extensions=extensions,
                                    output_format='html4').encode('utf-8')
    try:
      self._WriteTemplate('header.html')
      self.wfile.write(md_fragment)
      self._WriteTemplate('footer.html')
    except:
      raise

  def _DoNotFound(self):
    self.wfile.write('<html><body>%s not found</body></html>' % self.path)

  def _DoUnknown(self):
    self.wfile.write('<html><body>I do not know how to serve %s.</body>'
                       '</html>' % self.path)

  def _Read(self, relpath):
    assert not relpath.startswith(os.sep)
    path = os.path.join(self.server.top_level, relpath)
    with codecs.open(path, encoding='utf-8') as fp:
      return fp.read()

  def _WriteTemplate(self, template):
    contents = self._Read(os.path.join('tools', 'md_browser', template))
    self.wfile.write(contents.encode('utf-8'))


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
