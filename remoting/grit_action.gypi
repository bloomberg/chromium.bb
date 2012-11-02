# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is mostly a copy of base/grit_action.gypi . There are two
# differences:
#  1. Remoting resources are build with a different resource_ids file.
#  2. File specified by variable base_grit_grd_file is used to generate list of
#     inputs and outputs.

{
  'variables': {
    'grit_cmd': ['python', '<(DEPTH)/tools/grit/grit.py'],
  },
  'inputs': [
    '<(resource_ids_file)',
    '<(grit_grd_file)',
    '<!@pymod_do_main(grit_info <@(grit_defines) '
    '--inputs <(base_grit_grd_file) -fresource_ids)',
  ],
  'outputs': [
    '<!@pymod_do_main(grit_info <@(grit_defines) '
    '--outputs \'<(grit_out_dir)\' <(base_grit_grd_file) -fresource_ids)',
  ],
  'action': ['<@(grit_cmd)',
             '-i', '<(grit_grd_file)', 'build',
             '-fresource_ids',
             '-o', '<(grit_out_dir)',
             '<@(grit_defines)' ],
  'msvs_cygwin_shell': 0,
  'message': 'Generating resources from <(grit_grd_file)',
}
