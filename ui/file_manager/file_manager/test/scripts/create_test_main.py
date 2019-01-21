#!/usr/bin/env python

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Copies file_manager/main.html to file_manager/test.html.

Modifies it to be able to run the CrOS FileManager app
as a regular web page in a single renderer.
"""


import argparse
import json
import os
import re
import sys
import time
import xml.etree.ElementTree as ET


assert __name__ == '__main__'

parser = argparse.ArgumentParser()
parser.add_argument('--output')
args = parser.parse_args()

# If --output is not provided, write to local test.html.
output = args.output or os.path.abspath(
    os.path.join(sys.path[0], '../../test.html'))

# ROOT_SRC: Absolute path to src //ui/file_manager/file_manager.
# ROOT_GEN: Absolute path to $target_gen_dir of ROOT_SRC.
# ROOT    : Relative path from ROOT_GEN to ROOT_SRC.
ROOT_SRC = os.path.abspath(os.path.join(sys.path[0], '../..'))
ROOT_GEN = os.path.dirname(os.path.abspath(output))
ROOT = os.path.relpath(ROOT_SRC, ROOT_GEN) + '/'
POLYMER_1 = '../../../third_party/polymer/v1_0/components-chromium/'
ROOT_POLYMER = '%s/%s' % (ROOT_SRC, POLYMER_1)
# Replacement for chrome://resources/html/polymer.html.
POLYMER_IMPORTS = (
    '<script src="../../webui/resources/js/polymer_config.js"></script>\n'
    '<link rel="import" href="%spolymer/polymer.html">' % POLYMER_1)
scripts = []
GENERATED = 'Generated at %s by: %s' % (time.ctime(), sys.path[0])
GENERATED_HTML = '<!-- %s -->\n\n' % GENERATED


def read(path):
  with open(os.path.join(ROOT_SRC, path)) as f:
    return f.read()


def write(path, content):
  fullpath = os.path.join(ROOT_GEN, path)
  if not os.path.exists(os.path.dirname(fullpath)):
    os.makedirs(os.path.dirname(fullpath))
  with open(fullpath, 'w') as f:
    f.write(content)


def replaceline(f, match, lines):
  """Replace matching line in file with lines."""
  for i in range(len(f)):
    if match in f[i]:
      return f[:i] + lines + f[i+1:]
  return f


def includes2scripts(include_filename):
  """Convert <include src='foo'> to <script src='<prefix>foo'></script>."""
  scripts.append('<!-- %s -->' % include_filename)
  prefix = ROOT + include_filename[:include_filename.rindex('/')+1]
  f = read(include_filename).split('\n')
  for i in range(len(f)):
    l = f[i]
    # Join back any include with a line-break.
    if l == '// <include' and f[i+1].startswith('// src='):
      f[i+1] = l + f[i+1][2:]
      continue
    if l.startswith('// <include '):
      l = l.replace('// <include ', '<script ')
      # main.js should be defer.
      if 'src="main.js"' in l:
        l = l.replace('src="main.js"', 'src="main.js" defer')
      # Fix the path for scripts to be relative to ROOT.
      if 'src="../../' in l:
        l = l.replace('src="../../', 'src="' + ROOT)
      else:
        l = l.replace('src="', 'src="' + prefix)
      tag = l + '</script>'
      if tag not in scripts:
        scripts.append(tag)

# Update relative paths.
# Fix link to action_link.css and text_defaults.css.
# Fix stylesheet from extension.
main_html = (read('main.html')
             .replace('chrome://resources/css/action_link.css',
                      '../../webui/resources/css/action_link.css')
             .replace('<link rel="import" '
                      'href="chrome://resources/html/polymer.html">',
                      POLYMER_IMPORTS)
             .replace('href="', 'href="' + ROOT)
             .replace('src="', 'src="' + ROOT)
             .replace(ROOT + 'chrome://resources/css/text_defaults.css',
                      'test/gen/css/text_defaults.css')
             .split('\n'))

# Fix text_defaults.css.  Copy and replace placeholders.
text_defaults = (read('../../webui/resources/css/text_defaults.css')
                 .replace('$i18n{textDirection}', 'ltr')
                 .replace('$i18nRaw{fontFamily}', 'Roboto, sans-serif')
                 .replace('$i18nRaw{fontSize}', '75%'))
write('test/gen/css/text_defaults.css', text_defaults)

# Add scripts required for testing, and the test files (test/*.js).
scripts.append('<!-- required for testing -->')
scripts += ['<script src="%s%s"></script>' % (ROOT, s) for s in [
    'test/js/chrome_api_test_impl.js',
    '../../webui/resources/js/assert.js',
    '../../webui/resources/js/cr.js',
    '../../webui/resources/js/cr/event_target.js',
    '../../webui/resources/js/cr/ui/array_data_model.js',
    '../../webui/resources/js/load_time_data.js',
    '../../webui/resources/js/webui_resource_test.js',
    'test/js/strings.js',
    'common/js/files_app_entry_types.js',
    'common/js/util.js',
    'common/js/mock_entry.js',
    '../base/js/volume_manager_types.js',
    'background/js/volume_info_impl.js',
    'background/js/volume_info_list_impl.js',
    'background/js/volume_manager_impl.js',
    'background/js/mock_volume_manager.js',
    'foreground/js/constants.js',
    'test/js/chrome_file_manager_private_test_impl.js',
    'test/js/test_util.js',
] + ['test/' + s for s in os.listdir(os.path.join(ROOT_SRC, 'test'))
     if s.endswith('.js')]]

# Convert all includes from:
#  * foreground/js/main_scripts.js
#  * background/js/background_common_scripts.js
#  * background/js/background_scripts.js
# into <script> tags in main.html.
# Add polymer libs at start.
# Define FILE_MANAGER_ROOT which is required to locate test data files.
includes2scripts('foreground/js/main_scripts.js')
includes2scripts('background/js/background_common_scripts.js')
includes2scripts('background/js/background_scripts.js')

# test_util_base.js in background_common_scripts.js loads this at runtime.
# However, test/js/test_util.js copies some functions from it into its own
# test context, so provide it here.
scripts += ['<script src="%s%s"></script>' %
            (ROOT, 'background/js/runtime_loaded_test_util.js')]

main_html = replaceline(main_html, 'foreground/js/main_scripts.js', [
    "<script>var FILE_MANAGER_ROOT = '%s';</script>" % ROOT,
    ] + scripts)

# Get strings from grdp files.  Remove any ph/ex elements before getting text.
# Parse private_api_strings.cc to match the string name to the grdp message.
strings = {}
grdp_files = [
    '../../../chrome/app/chromeos_strings.grdp',
    '../../../chrome/app/file_manager_strings.grdp',
    ]
resource_bundle = {}
for grdp in grdp_files:
  for msg in ET.fromstring(read(grdp)):
    for ph in msg.findall('ph'):
      for ex in ph.findall('ex'):
        ph.remove(ex)
    resource_bundle[msg.attrib['name']] = ''.join(msg.itertext()).strip()
private_api_strings = read('../../../chrome/browser/chromeos/'
                           'file_manager/file_manager_string_util.cc')
for m in re.finditer(r'SET_STRING\(\"(.*?)\",\s+(\w+)\);', private_api_strings):
  strings[m.group(1)] = resource_bundle.get(m.group(2), m.group(2))


def elements_path(elements_filename):
  return '="../../../../%sforeground/elements/%s' % (ROOT, elements_filename)

# Fix relative file paths in elements_bundle.html
# Load QuickView in iframe rather than webview.
# Change references in files_quick_view.html to use updated
# files_safe_media.html which will use webview rather than iframe,
# and sets src directly on iframe.
for filename, substitutions in (
    ('test/js/strings.js', (
        ('$GRDP', json.dumps(strings, sort_keys=True, indent=2)),
    )),
    ('foreground/elements/elements_bundle.html', (
        ('chrome://resources/polymer/v1_0/', ROOT_POLYMER),
        ('="files_ripple', elements_path('files_ripple')),
        ('="files_toast', elements_path('files_toast')),
        ('="files_toggle_ripple', elements_path('files_toggle_ripple')),
        ('="files_tooltip', elements_path('files_tooltip')),
        ('="icons', elements_path('icons')),
    )),
    ('foreground/js/elements_importer.js', (
        ("= 'foreground", "= 'test/gen/foreground"),
    )),
    ('foreground/elements/files_quick_view.html', (
        ('chrome://resources/polymer/v1_0/', ROOT_POLYMER),
        ('="files_icon', elements_path('files_icon')),
        ('="files_metadata', elements_path('files_metadata')),
        ('="files_tooltip', elements_path('files_tooltip')),
        ('="files_quick', elements_path('files_quick')),
        ('webview', 'iframe'),
    )),
    ('foreground/elements/files_safe_media.html', (('webview', 'iframe'),)),
    ('foreground/elements/files_safe_media.js', (
        ("'foreground/elements", "'%sforeground/elements" % ROOT),
        ("'webview'", "'iframe'"),
        ("'contentload'", "'load'"),
        ('this.webview_.contentWindow.postMessage(data, FILES_APP_ORIGIN);',
         ('this.webview_.contentWindow.content.type = this.type;'
          'this.webview_.contentWindow.content.src = this.src;')),
    )),
    ('foreground/elements/icons.html', (
        ('chrome://resources/polymer/v1_0/', ROOT_POLYMER),
    )),
    ):
  buf = read(filename)
  for old, new in substitutions:
    buf = buf.replace(old, new)
  write('test/gen/' + filename, buf)
  main_html = replaceline(main_html, filename,
                          ['<script src="test/gen/%s"></script>' % filename])

test_html = GENERATED_HTML + '\n'.join(main_html)
write('test.html', test_html)
