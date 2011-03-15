#!/usr/bin/python2.4
# Copyright 2009 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script does substitution on a list of files for
# version-specific information relating to the plugin.

import os.path
import re
import sys

script_dir = os.path.join(os.path.dirname(sys.argv[0]))
gflags_dir = os.path.normpath(
  os.path.join(script_dir, '..', 'third_party', 'gflags', 'python'))
sys.path.append(gflags_dir)

import gflags

FLAGS = gflags.FLAGS
gflags.DEFINE_boolean('description', False,
                      'Print out the plugin description and exit.')

gflags.DEFINE_boolean('commaversion', False,
                      'Print out the plugin version with commas and exit.')

gflags.DEFINE_string('set_name', '',
                     'Sets the plugin name to use.')

gflags.DEFINE_string('set_version', '',
                     'Sets the plugin version to use.')

gflags.DEFINE_string('set_npapi_filename', '',
                     'Sets the plugin NPAPI filename to use.')

gflags.DEFINE_string('set_npapi_mimetype', '',
                     'Sets the plugin NPAPI mimetype to use.')

gflags.DEFINE_string('set_activex_hostcontrol_clsid', '',
                     'Sets the ActiveX HostControl\'s CLSID to use.')

gflags.DEFINE_string('set_activex_typelib_clsid', '',
                     'Sets the ActiveX TypeLib\'s CLSID to use.')

gflags.DEFINE_string('set_activex_hostcontrol_name', '',
                     'Sets the ActiveX HostControl\'s name to use.')

gflags.DEFINE_string('set_activex_typelib_name', '',
                     'Sets the ActiveX TypeLib\'s name to use.')

gflags.DEFINE_string('set_o3d_plugin_breakpad_url', '',
                     'Sets the o3d Crash Dump Upload URL to use.')

def DoReplace(in_filename, out_filename, replacements):
  '''Replace the version placeholders in the given filename with the
     replacements.'''
  if not os.path.exists(in_filename):
    raise Exception(r'''Input template file %s doesn't exist.''' % in_filename)
  input_file = open(in_filename, 'r')
  input = input_file.read()
  input_file.close()
  for (source, target) in replacements:
    input = re.sub(source, target, input)

  output_file = open(out_filename, 'wb')
  output_file.write(input)
  output_file.close()

def main(argv):
  try:
    files = FLAGS(argv)  # Parse flags
  except gflags.FlagsError, e:
    print '%s.\nUsage: %s [<options>] [<input_file> <output_file>]\n%s' % \
          (e, sys.argv[0], FLAGS)
    sys.exit(1)

  # Strip off argv[0]
  files = files[1:]

  # This name is used by Javascript to find the plugin therefore it must
  # not change. If you change this you must change the name in
  # samples/o3djs/util.js but be aware, changing the name
  # will break all apps that use o3d on the web.
  O3D_PLUGIN_NAME = FLAGS.set_name
  O3D_PLUGIN_VERSION = FLAGS.set_version
  O3D_PLUGIN_VERSION_COMMAS = O3D_PLUGIN_VERSION.replace('.', ',')
  O3D_PLUGIN_DESCRIPTION = '%s version:%s' % (O3D_PLUGIN_NAME,
                                              O3D_PLUGIN_VERSION)
  O3D_PLUGIN_NPAPI_FILENAME = FLAGS.set_npapi_filename
  O3D_PLUGIN_NPAPI_MIMETYPE = FLAGS.set_npapi_mimetype
  O3D_PLUGIN_ACTIVEX_HOSTCONTROL_CLSID = FLAGS.set_activex_hostcontrol_clsid
  O3D_PLUGIN_ACTIVEX_TYPELIB_CLSID = FLAGS.set_activex_typelib_clsid
  O3D_PLUGIN_ACTIVEX_HOSTCONTROL_NAME = FLAGS.set_activex_hostcontrol_name
  O3D_PLUGIN_ACTIVEX_TYPELIB_NAME = FLAGS.set_activex_typelib_name
  O3D_PLUGIN_BREAKPAD_URL = FLAGS.set_o3d_plugin_breakpad_url

  if FLAGS.description:
    print '%s' % O3D_PLUGIN_DESCRIPTION
    sys.exit(0)

  if FLAGS.commaversion:
    print '%s' % O3D_PLUGIN_VERSION_COMMAS
    sys.exit(0)

  plugin_replace_strings = [
      ('@@@PluginName@@@', O3D_PLUGIN_NAME),
      ('@@@ProductVersionCommas@@@', O3D_PLUGIN_VERSION_COMMAS),
      ('@@@ProductVersion@@@', O3D_PLUGIN_VERSION),
      ('@@@PluginDescription@@@', O3D_PLUGIN_DESCRIPTION),
      ('@@@PluginNpapiFilename@@@', O3D_PLUGIN_NPAPI_FILENAME),
      ('@@@PluginNpapiMimeType@@@', O3D_PLUGIN_NPAPI_MIMETYPE),
      ('@@@PluginActiveXHostControlClsid@@@',
           O3D_PLUGIN_ACTIVEX_HOSTCONTROL_CLSID),
      ('@@@PluginActiveXTypeLibClsid@@@', O3D_PLUGIN_ACTIVEX_TYPELIB_CLSID),
      ('@@@PluginActiveXHostControlName@@@',
           O3D_PLUGIN_ACTIVEX_HOSTCONTROL_NAME),
      ('@@@PluginActiveXTypeLibName@@@', O3D_PLUGIN_ACTIVEX_TYPELIB_NAME),
      ('@@@PluginBreakpadURL@@@', O3D_PLUGIN_BREAKPAD_URL),
  ]

  if len(files) == 2:
    DoReplace(files[0], files[1], plugin_replace_strings)
  elif len(files) > 0:
    raise Exception(r'You must supply and input and output filename for '
                    r'replacement.')

if __name__ == '__main__':
  main(sys.argv)
