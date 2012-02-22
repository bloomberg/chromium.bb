#!/usr/bin/env/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Server for viewing the compiled C++ code from tools/json_schema_compiler.
"""

import cc_generator
import code
import cpp_type_generator
import cpp_util
import h_generator
import json
import model
import optparse
import os
import sys
import urlparse
from highlighters import (
    pygments_highlighter, none_highlighter, hilite_me_highlighter)
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

class CompilerHandler(BaseHTTPRequestHandler):
  """A HTTPRequestHandler that outputs the result of tools/json_schema_compiler.
  """
  def do_GET(self):
    parsed_url = urlparse.urlparse(self.path)
    request_path = self._GetRequestPath(parsed_url)

    chromium_favicon = 'http://codereview.chromium.org/static/favicon.ico'

    head = code.Code()
    head.Append('<link rel="icon" href="%s">' % chromium_favicon)
    head.Append('<link rel="shortcut icon" href="%s">' % chromium_favicon)

    body = code.Code()

    try:
      if parsed_url.path.split('/')[1] == 'nav':
        self._ListFoldersAndJsons(parsed_url, head, body)
      elif os.path.isdir(request_path):
        self._ShowFrameset(parsed_url, head, body)
      else:
        self._ShowCompiledFile(parsed_url, head, body)
    finally:
      self.wfile.write('<html><head>')
      self.wfile.write(head.Render())
      self.wfile.write('</head><body>')
      self.wfile.write(body.Render())
      self.wfile.write('</body></html>')

  def _GetRequestPath(self, parsed_url, strip_nav=False):
    """Get the relative path from the current directory to the requested file.
    """
    path = parsed_url.path
    if strip_nav:
      path = parsed_url.path.replace('/nav', '')
    return os.path.normpath(os.curdir + path)

  def _ShowFrameset(self, parsed_url, head, body):
    """Show the previewer frame structure.

    Frames can be accessed in javascript (from a iframe) by using
    parent.document.getElementById(|iframe id|).

    Code panes are set by onClick events of links in the nav pane.
    """
    (head.Append('<style>')
         .Append('body {')
         .Append('  margin: 0;')
         .Append('}')
         .Append('iframe {')
         .Append('  border: none;')
         .Append('  height: 100%;')
         .Append('}')
         .Append('#nav_pane {')
         .Append('  width: 20%;')
         .Append('}')
         .Append('#cc_pane {')
         .Append('  width: 40%;')
         .Append('}')
         .Append('#h_pane {')
         .Append('  width: 40%;')
         .Append('}')
         .Append('</style>')
    )
    body.Append(
        '<iframe src="/nav/%(path)s?%(query)s"></iframe>'
        '<iframe id="h_pane"></iframe>'
        '<iframe id="cc_pane"></iframe>' % {
            'path': parsed_url.path,
            'query': parsed_url.query
        }
    )

  def _ShowCompiledFile(self, parsed_url, head, body):
    """Show the compiled version of a json file given the path to the compiled
    file.
    """

    api_model = model.Model()

    request_path = self._GetRequestPath(parsed_url)
    (file_root, file_ext) = os.path.splitext(request_path)
    (filedir, filename) = os.path.split(file_root)
    json_file_path = os.path.normpath(file_root + '.json')

    try:
      # Get main json file
      with open(json_file_path) as json_file:
        api_defs = json.loads(json_file.read())
      namespace = api_model.AddNamespace(api_defs[0], json_file_path)
      if not namespace:
        body.concat("<pre>Target file %s is marked nocompile</pre>" %
            json_file_path)
        return
      type_generator = cpp_type_generator.CppTypeGenerator(
         'preview::api', namespace, cpp_util.Classname(filename).lower())

      # Get json file depedencies
      for dependency in api_defs[0].get('dependencies', []):
        json_file_path = os.path.join(filedir, dependency + '.json')
        with open(json_file_path) as json_file:
          api_defs = json.loads(json_file.read())
        referenced_namespace = api_model.AddNamespace(api_defs[0],
            json_file_path)
        if referenced_namespace:
          type_generator.AddNamespace(referenced_namespace,
              cpp_util.Classname(referenced_namespace.name).lower())

      # Generate code
      if file_ext == '.h':
        cpp_code = (h_generator.HGenerator(namespace, type_generator)
            .Generate().Render())
      elif file_ext == '.cc':
        cpp_code = (cc_generator.CCGenerator(namespace, type_generator)
            .Generate().Render())
      else:
        self.send_error(404, "File not found: %s" % request_path)
        return

      # Do highlighting on the generated code
      (highlighter_param, style_param) = self._GetHighlighterParams(parsed_url)
      head.Append('<style>' +
          self.server.highlighters[highlighter_param].GetCSS(style_param) +
          '</style>')
      body.Append(self.server.highlighters[highlighter_param]
          .GetCodeElement(cpp_code, style_param))
    except IOError:
      self.send_error(404, "File not found: %s" % request_path)
      return
    except (TypeError, KeyError, AttributeError,
        AssertionError, NotImplementedError) as error:
      body.Append('<pre>')
      body.Append('compiler error: ' + str(error))
      body.Append('Check server log for more details')
      body.Append('</pre>')
      raise

  def _GetHighlighterParams(self, parsed_url):
    """Get the highlighting parameters from a parsed url.
    """
    query_dict = urlparse.parse_qs(parsed_url.query)
    return (query_dict.get('highlighter', ['pygments'])[0],
        query_dict.get('style', ['colorful'])[0])

  def _ListFoldersAndJsons(self, parsed_url, head, body):
    """List folders and .json files in given directory.
    """
    request_path = self._GetRequestPath(parsed_url, strip_nav=True)
    (highlighter_param, style_param) = self._GetHighlighterParams(parsed_url)

    body.Append('<select onChange="'
        'document.location.href=\'%s?highlighter=\' + this.value">' %
            parsed_url.path)
    for key, value in self.server.highlighters.items():
      body.Append('<option %s value="%s">%s</option>' %
          ('selected="true"' if key == highlighter_param else '',
              key, value.DisplayName()))
    body.Append('</select><br />')

    highlighter = self.server.highlighters[highlighter_param]
    styles = sorted(highlighter.GetStyles())
    if styles:
      body.Append('<select onChange="'
          'document.location.href=\'%s?highlighter=%s&style=\' '
          '+ this.value">' % (parsed_url.path, highlighter_param))
      for style in styles:
        body.Append('<option %s>%s</option>' %
            ('selected="true"' if style == style_param else '', style))

      body.Append('</select><br />')

    if not os.path.samefile(os.curdir, request_path):
      body.Append('<a href="%s?%s">../</a><br />' %
          (os.path.split(os.path.normpath(parsed_url.path))[0],
              parsed_url.query))
    files = os.listdir(request_path)
    for filename in sorted(files):
      full_path = os.path.join(request_path, filename)
      (file_root, file_ext) = os.path.splitext(full_path)
      if os.path.isdir(full_path):
        body.Append('<a href="/nav/%s/">%s/</a><br />' %
            (full_path, filename))
      elif file_ext == '.json':
        body.Append('<a href="#%(file_root)s" onClick="'
            'parent.document.getElementById(\'cc_pane\').src='
                '\'/%(file_root)s.cc?%(query)s\'; '
            'parent.document.getElementById(\'h_pane\').src='
                '\'/%(file_root)s.h?%(query)s\';'
            '">%(filename)s</a><br />' % {
                'file_root': file_root,
                'filename': filename,
                'query': parsed_url.query,
        })

class PreviewHTTPServer(HTTPServer, object):
  def __init__(self, server_address, handler, highlighters):
    super(PreviewHTTPServer, self).__init__(server_address, handler)
    self.highlighters = highlighters


if __name__ == '__main__':
  parser = optparse.OptionParser(
      description='Runs a server to preview the json_schema_compiler output.',
      usage='usage: %prog [option]...')
  parser.add_option('-p', '--port', default=8000,
      help='port to run the server on')

  (opts, argv) = parser.parse_args()

  try:
    print('Starting httpserver on port %d' % opts.port)
    server = PreviewHTTPServer(('', opts.port), CompilerHandler,
      {
        'pygments': pygments_highlighter.PygmentsHighlighter(),
        'hilite': hilite_me_highlighter.HiliteMeHighlighter(),
        'none': none_highlighter.NoneHighlighter(),
      })
    server.serve_forever()
  except KeyboardInterrupt:
    print('Shutting down server')
    server.socket.close()
