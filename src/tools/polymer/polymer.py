# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates Polymer3 UI elements (using JS modules) from existing Polymer2
# elements (using HTML imports). This is useful for avoiding code duplication
# while Polymer2 to Polymer3 migration is in progress.
#
# Variables:
#   html_file:
#     The input Polymer2 HTML file to be processed.
#
#   js_file:
#     The input Polymer2 JS file to be processed, or the name of the output JS
#     file when no input JS file exists (see |html_type| below).
#
#   in_folder:
#     The folder where |html_file| and |js_file| (when it exists) reside.
#
#   out_folder:
#     The output folder for the generated Polymer JS file.
#
#   html_type:
#     Specifies the type of the |html_file| such that the script knows how to
#     process the |html_file|. Available values are:
#       dom-module: A file holding a <dom-module> for a UI element (this is
#                   the majority case). Note: having multiple <dom-module>s
#                   within a single HTML file is not currently supported
#       style-module: A file holding a shared style <dom-module>
#                     (no corresponding Polymer2 JS file exists)
#       custom-style: A file holding a <custom-style> (usually a *_vars_css.html
#                     file, no corresponding Polymer2 JS file exists)
#       iron-iconset: A file holding one or more <iron-iconset-svg> instances
#                     (no corresponding Polymer2 JS file exists)
#       v3-ready: A file holding HTML that is already written for Polymer3. A
#                 Polymer3 JS file already exists for such cases. In this mode
#                 HTML content is simply pasted within the JS file. This mode
#                 will be the only supported mode after migration finishes.
#
#   namespace_rewrites:
#     A list of string replacements for replacing global namespaced references
#     with explicitly imported dependencies in the generated JS module.
#     For example "cr.foo.Bar|Bar" will replace all occurrences of "cr.foo.Bar"
#     with "Bar".
#
#   auto_imports:
#     A list of of auto-imports, to inform the script on which variables to
#     import from a JS module. For example "ui/webui/foo/bar/baz.html|Foo,Bar"
#     will result in something like "import {Foo, Bar} from ...;" when
#     encountering any dependency to that file.

import argparse
import os
import re
import sys

_CWD = os.getcwd()
_ROOT = os.path.normpath(os.path.join(_CWD, '..', '..'))

POLYMER_V1_DIR = 'third_party/polymer/v1_0/components-chromium/'
POLYMER_V3_DIR = 'third_party/polymer/v3_0/components-chromium/'

# Rewrite rules for replacing global namespace references like "cr.ui.Foo", to
# "Foo" within a generated JS module. Populated from command line arguments.
_namespace_rewrites = {}

# Auto-imports map, populated from command line arguments. Specifies which
# variables to import from a given dependency. For example this is used to
# import |FocusOutlineManager| whenever a dependency to
# ui/webui/resources/html/cr/ui/focus_outline_manager.html is encountered.
_auto_imports = {}

_chrome_redirects = {
  'chrome://resources/polymer/v1_0/': POLYMER_V1_DIR,
  'chrome://resources/html/': 'ui/webui/resources/html/',
}

_chrome_reverse_redirects = {
  POLYMER_V3_DIR: 'chrome://resources/polymer/v3_0/',
  'ui/webui/resources/': 'chrome://resources/',
}


# Helper class for converting dependencies expressed in HTML imports, to JS
# imports. |to_js_import()| is the only public method exposed by this class.
# Internally an HTML import path is
#
# 1) normalized, meaning converted from a chrome or relative URL to to an
#    absolute path starting at the repo's root
# 2) converted to an equivalent JS normalized path
# 3) de-normalized, meaning converted back to a relative or chrome URL
# 4) converted to a JS import statement
class Dependency:
  def __init__(self, src, dst):
    self.html_file = src
    self.html_path = dst

    self.input_format = (
        'chrome' if self.html_path.startswith('chrome://') else 'relative')
    self.output_format = self.input_format

    self.html_path_normalized = self._to_html_normalized()
    self.js_path_normalized = self._to_js_normalized()
    self.js_path = self._to_js()

  def _to_html_normalized(self):
    if self.input_format == 'chrome':
      self.html_path_normalized = self.html_path
      for r in _chrome_redirects:
        if self.html_path.startswith(r):
          self.html_path_normalized = (
              self.html_path.replace(r, _chrome_redirects[r]))
          break
      return self.html_path_normalized

    input_dir = os.path.relpath(os.path.dirname(self.html_file), _ROOT)
    return os.path.normpath(
        os.path.join(input_dir, self.html_path)).replace("\\", "/")

  def _to_js_normalized(self):
    if re.match(POLYMER_V1_DIR, self.html_path_normalized):
      return (self.html_path_normalized
          .replace(POLYMER_V1_DIR, POLYMER_V3_DIR)
          .replace(r'.html', '.js'))

    if self.html_path_normalized == 'ui/webui/resources/html/polymer.html':
      self.output_format = 'chrome'
      return POLYMER_V3_DIR + 'polymer/polymer_bundled.min.js'

    if re.match(r'ui/webui/resources/html/', self.html_path_normalized):
      return (self.html_path_normalized
          .replace(r'ui/webui/resources/html/', 'ui/webui/resources/js/')
          .replace(r'.html', '.m.js'))

    return self.html_path_normalized.replace(r'.html', '.m.js')

  def _to_js(self):
    js_path = self.js_path_normalized

    if self.output_format == 'chrome':
      for r in _chrome_reverse_redirects:
        if self.js_path_normalized.startswith(r):
          js_path = self.js_path_normalized.replace(
              r, _chrome_reverse_redirects[r])
          break
      return js_path

    input_dir = os.path.relpath(os.path.dirname(self.html_file), _ROOT)
    relpath = os.path.relpath(
        self.js_path_normalized, input_dir).replace("\\", "/")
    # Prepend "./" if |relpath| refers to a relative subpath, that is not "../".
    # This prefix is required for JS Modules paths.
    if not relpath.startswith('.'):
      relpath = './' + relpath

    return relpath

  def to_js_import(self, auto_imports):
    if self.html_path_normalized in auto_imports:
      imports = auto_imports[self.html_path_normalized]
      return 'import {%s} from \'%s\';' % (', '.join(imports), self.js_path)

    return 'import \'%s\';' % self.js_path


def _extract_dependencies(html_file):
  with open(html_file, 'r') as f:
    lines = f.readlines()
    deps = []
    for line in lines:
      match = re.search(r'\s*<link rel="import" href="(.*)"', line)
      if match:
        deps.append(match.group(1))
  return deps;


def _generate_js_imports(html_file):
  return map(
      lambda dep: Dependency(html_file, dep).to_js_import(_auto_imports),
      _extract_dependencies(html_file))


def _extract_dom_module_id(html_file):
  with open(html_file, 'r') as f:
    contents = f.read()
    match = re.search(r'\s*<dom-module id="(.*)"', contents)
    assert match
    return match.group(1)


def _extract_template(html_file, html_type):
  if html_type == 'v3-ready':
    with open(html_file, 'r') as f:
      return f.read()

  if html_type == 'dom-module':
    with open(html_file, 'r') as f:
      lines = f.readlines()
      start_line = -1
      end_line = -1
      for i, line in enumerate(lines):
        if re.match(r'\s*<dom-module ', line):
          assert start_line == -1
          assert end_line == -1
          assert re.match(r'\s*<template', lines[i + 1])
          start_line = i + 2;
        if re.match(r'\s*</dom-module>', line):
          assert start_line != -1
          assert end_line == -1
          assert re.match(r'\s*</template>', lines[i - 2])
          assert re.match(r'\s*<script ', lines[i - 1])
          end_line = i - 3;
    return '\n' + ''.join(lines[start_line:end_line + 1])

  if html_type == 'style-module':
    with open(html_file, 'r') as f:
      lines = f.readlines()
      start_line = -1
      end_line = -1
      for i, line in enumerate(lines):
        if re.match(r'\s*<dom-module ', line):
          assert start_line == -1
          assert end_line == -1
          assert re.match(r'\s*<template', lines[i + 1])
          start_line = i + 1;
        if re.match(r'\s*</dom-module>', line):
          assert start_line != -1
          assert end_line == -1
          assert re.match(r'\s*</template>', lines[i - 1])
          end_line = i - 1;
    return '\n' + ''.join(lines[start_line:end_line + 1])


  if html_type == 'iron-iconset':
    templates = []
    with open(html_file, 'r') as f:
      lines = f.readlines()
      start_line = -1
      end_line = -1
      for i, line in enumerate(lines):
        if re.match(r'\s*<iron-iconset-svg ', line):
          assert start_line == -1
          assert end_line == -1
          start_line = i;
        if re.match(r'\s*</iron-iconset-svg>', line):
          assert start_line != -1
          assert end_line == -1
          end_line = i
          templates.append(''.join(lines[start_line:end_line + 1]))
          # Reset indices.
          start_line = -1
          end_line = -1
    return '\n' + ''.join(templates)


  assert html_type == 'custom-style'
  with open(html_file, 'r') as f:
    lines = f.readlines()
    start_line = -1
    end_line = -1
    for i, line in enumerate(lines):
      if re.match(r'\s*<custom-style>', line):
        assert start_line == -1
        assert end_line == -1
        start_line = i;
      if re.match(r'\s*</custom-style>', line):
        assert start_line != -1
        assert end_line == -1
        end_line = i;

  return '\n' + ''.join(lines[start_line:end_line + 1])


# Replace various global references with their non-namespaced version, for
# example "cr.ui.Foo" becomes "Foo".
def _rewrite_namespaces(string):
  for rewrite in _namespace_rewrites:
    string = string.replace(rewrite, _namespace_rewrites[rewrite])
  return string


def _process_v3_ready(js_file, html_file):
  # Extract HTML template and place in JS file.
  html_template = _extract_template(html_file, 'v3-ready')

  with open(js_file) as f:
    lines = f.readlines()

  HTML_TEMPLATE_REGEX = '{__html_template__}'
  for i, line in enumerate(lines):
    line = line.replace(HTML_TEMPLATE_REGEX, html_template)
    lines[i] = line

  out_filename = os.path.basename(js_file)
  return lines, out_filename


def _process_dom_module(js_file, html_file):
  html_template = _extract_template(html_file, 'dom-module')
  js_imports = _generate_js_imports(html_file)

  IIFE_OPENING = '(function() {\n'
  IIFE_OPENING_ARROW = '(() => {\n'
  IIFE_CLOSING = '})();'

  with open(js_file) as f:
    lines = f.readlines()

  imports_added = False
  iife_found = False

  for i, line in enumerate(lines):
    if not imports_added:
      if line.startswith(IIFE_OPENING) or line.startswith(IIFE_OPENING_ARROW):
        # Replace the IIFE opening line with the JS imports.
        line = '\n'.join(js_imports) + '\n\n'
        imports_added = True
        iife_found = True
      elif line.startswith('Polymer({\n'):
        # Place the JS imports right before the opening "Polymer({" line.
        line = line.replace(
            r'Polymer({', '%s\n\nPolymer({' % '\n'.join(js_imports))
        imports_added = True

    # Place the HTML content right after the opening "Polymer({" line.
    # Note: There is currently an assumption that only one Polymer() declaration
    # exists per file.
    line = line.replace(
        r'Polymer({',
        'Polymer({\n  _template: html`%s`,' % html_template)

    if line.startswith('cr.exportPath('):
      line = ''

    line = _rewrite_namespaces(line)
    lines[i] = line

  if iife_found:
    last_line = lines[-1]
    assert last_line.startswith(IIFE_CLOSING), 'Could not detect IIFE closing'
    lines[-1] = ''

  # Use .m.js extension for the generated JS file, since both files need to be
  # served by a chrome:// URL side-by-side.
  out_filename = os.path.basename(js_file).replace('.js', '.m.js')
  return lines, out_filename

def _process_style_module(js_file, html_file):
  html_template = _extract_template(html_file, 'style-module')
  js_imports = _generate_js_imports(html_file)

  style_id = _extract_dom_module_id(html_file)

  js_template = \
"""%(js_imports)s
const styleElement = document.createElement('dom-module');
styleElement.innerHTML = `%(html_template)s`;
styleElement.register('%(style_id)s');""" % {
  'js_imports': '\n'.join(js_imports),
  'html_template': html_template,
  'style_id': style_id,
}

  out_filename = os.path.basename(js_file)
  return js_template, out_filename


def _process_custom_style(js_file, html_file):
  html_template = _extract_template(html_file, 'custom-style')
  js_imports = _generate_js_imports(html_file)

  js_template = \
"""%(js_imports)s
const $_documentContainer = document.createElement('template');
$_documentContainer.innerHTML = `%(html_template)s`;
document.head.appendChild($_documentContainer.content);""" % {
  'js_imports': '\n'.join(js_imports),
  'html_template': html_template,
}

  out_filename = os.path.basename(js_file)
  return js_template, out_filename

def _process_iron_iconset(js_file, html_file):
  html_template = _extract_template(html_file, 'iron-iconset')
  js_imports = _generate_js_imports(html_file)

  js_template = \
"""%(js_imports)s
const template = html`%(html_template)s`;
document.head.appendChild(template.content);
""" % {
  'js_imports': '\n'.join(js_imports),
  'html_template': html_template,
}

  out_filename = os.path.basename(js_file)
  return js_template, out_filename


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--js_file', required=True)
  parser.add_argument('--html_file', required=True)
  parser.add_argument('--namespace_rewrites', required=False, nargs="*")
  parser.add_argument('--auto_imports', required=False, nargs="*")
  parser.add_argument(
      '--html_type', choices=['dom-module', 'style-module', 'custom-style',
      'iron-iconset', 'v3-ready'],
      required=True)
  args = parser.parse_args(argv)

  # Extract namespace rewrites from arguments.
  if args.namespace_rewrites:
    for r in args.namespace_rewrites:
      before, after = r.split('|')
      _namespace_rewrites[before] = after

  # Extract automatic imports from arguments.
  if args.auto_imports:
    for entry in args.auto_imports:
      path, imports = entry.split('|')
      _auto_imports[path] = imports.split(',')


  in_folder = os.path.normpath(os.path.join(_CWD, args.in_folder))
  out_folder = os.path.normpath(os.path.join(_CWD, args.out_folder))

  js_file = os.path.join(in_folder, args.js_file)
  html_file = os.path.join(in_folder, args.html_file)

  result = ()
  if args.html_type == 'dom-module':
    result = _process_dom_module(js_file, html_file)
  if args.html_type == 'style-module':
    result = _process_style_module(js_file, html_file)
  elif args.html_type == 'custom-style':
    result = _process_custom_style(js_file, html_file)
  elif args.html_type == 'iron-iconset':
    result = _process_iron_iconset(js_file, html_file)
  elif args.html_type == 'v3-ready':
    result = _process_v3_ready(js_file, html_file)

  # Reconstruct file.
  with open(os.path.join(out_folder, result[1]), 'w') as f:
    for l in result[0]:
      f.write(l)
  return


if __name__ == '__main__':
  main(sys.argv[1:])
