#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Driver of srpcgen for ppapi_proxy sources.

This must be run after modifying, adding, or removing .srpc files.
The lists of .srpc files are in this source file.
"""

import filecmp
import optparse
import os
import shutil
import subprocess
import sys
import tempfile

# This lists all the units (one header file, one source file) to generate.
# There is a trusted pair of files and an untrusted pair of files for each.
# Each element has these keys:
#       trusted_is_client       True if the trusted side is the client
#                               and the untrusted side is the server;
#                               False if the untrusted side is the client
#                               and the trusted side is the server.
#       name                    Prefix of the main class name, suffixed
#                               by Server or Client.
#       file_basename           Prefix of the output file names, suffixed
#                               by .h, _server.cc, _client.cc.
#       srpc_files              List of .srpc input file names.
#       client_thread_check     (optional) True if the client side should
#                               be generated with --thread-check.
#                               This asserts that calls are on the main thread.
all_units = [
    {'trusted_is_client': True,
     'name': 'PppRpcs',
     'file_basename': 'ppp_rpc',
     'srpc_files': [
         'completion_callback.srpc',
         'ppp.srpc',
         'ppp_audio.srpc',
         'ppp_find.srpc',
         'ppp_input_event.srpc',
         'ppp_instance.srpc',
         'ppp_messaging.srpc',
         'ppp_mouse_lock.srpc',
         'ppp_printing.srpc',
         'ppp_scrollbar.srpc',
         'ppp_selection.srpc',
         'ppp_widget.srpc',
         'ppp_zoom.srpc',
         ]},
    {'trusted_is_client': False,
     'client_thread_check': True,
     'name': 'PpbRpcs',
     'file_basename': 'ppb_rpc',
     'srpc_files': [
         'ppb.srpc',
         'ppb_audio.srpc',
         'ppb_audio_config.srpc',
         'ppb_core.srpc',
         'ppb_file_io.srpc',
         'ppb_file_ref.srpc',
         'ppb_file_system.srpc',
         'ppb_find.srpc',
         'ppb_font.srpc',
         'ppb_fullscreen.srpc',
         'ppb_gamepad.srpc',
         'ppb_graphics_2d.srpc',
         'ppb_graphics_3d.srpc',
         'ppb_host_resolver_private.srpc',
         'ppb_image_data.srpc',
         'ppb_input_event.srpc',
         'ppb_instance.srpc',
         'ppb_messaging.srpc',
         'ppb_mouse_cursor.srpc',
         'ppb_mouse_lock.srpc',
         'ppb_net_address_private.srpc',
         'ppb_pdf.srpc',
         'ppb_scrollbar.srpc',
         'ppb_tcp_server_socket_private.srpc',
         'ppb_tcp_socket_private.srpc',
         'ppb_testing.srpc',
         'ppb_udp_socket_private.srpc',
         'ppb_url_loader.srpc',
         'ppb_url_request_info.srpc',
         'ppb_url_response_info.srpc',
         'ppb_websocket.srpc',
         'ppb_widget.srpc',
         'ppb_zoom.srpc',
         ]},
    {'trusted_is_client': False,
     'name': 'PpbUpcalls',
     'file_basename': 'upcall',
     'srpc_files': [
         'upcall.srpc',
         ]},
    ]

def GeneratorForUnit(options, unit, is_trusted, output_dir):
  header_file_name = unit['file_basename'] + '.h'
  server_file_name = unit['file_basename'] + '_server.cc'
  client_file_name = unit['file_basename'] + '_client.cc'
  header_guard = 'GEN_PPAPI_PROXY_%s_H_' % unit['file_basename'].upper()

  is_client = is_trusted == unit['trusted_is_client']

  thread_check = unit.get('client_thread_check', False) == is_client

  header_dir = 'trusted' if is_trusted else 'untrusted'

  header = os.path.join(header_dir, 'srpcgen', header_file_name)
  source = client_file_name if is_client else server_file_name

  command = [
      sys.executable, options.srpcgen,
      '--ppapi',
      '--include=' + '/'.join([header_dir, 'srpcgen', header_file_name]),
      '-c' if is_client else '-s',
      ]
  if thread_check:
    command.append('--thread-check')
  command += [
      unit['name'],
      header_guard,
      os.path.join(output_dir, header),
      os.path.join(output_dir, source),
      ]
  command += [os.path.join(options.source_dir, file)
              for file in unit['srpc_files']]

  return command, [header, source]

def RunOneUnit(options, unit, is_trusted):
  result = 0
  if options.diff_mode:
    temp_dir = tempfile.mkdtemp(prefix='srpcdiff')
    try:
      command, files = GeneratorForUnit(options, unit, is_trusted, temp_dir)
      result = subprocess.call(command)
      if result != 0:
        print 'Command failed: ' + ' '.join(command)
      else:
        for file in files:
          output_file = os.path.join(options.output_dir, file)
          generated_file = os.path.join(temp_dir, file)
          if not filecmp.cmp(output_file, generated_file, shallow=False):
            print '%s is out of date' % output_file
            result = 1
    finally:
      shutil.rmtree(temp_dir, ignore_errors=True)
  else:
    command, _ = GeneratorForUnit(options, unit, is_trusted, options.output_dir)
    print 'Run: ' + ' '.join(command)
    if not options.dry_run:
      result = subprocess.call(command)
  return result

def RunUnits(options, is_trusted):
  result = 0
  for unit in all_units:
    this_result = RunOneUnit(options, unit, is_trusted)
    if this_result != 0:
      print 'Error %d on %s.' % (this_result, unit['name'])
      result = this_result
  return result

def RunAll(options):
  trusted_result = RunUnits(options, True)
  untrusted_result = RunUnits(options, False)
  return trusted_result or untrusted_result

def Main(argv):
  parser = optparse.OptionParser(usage='Usage: %prog [options]')
  parser.add_option('-o', '--output_dir',
                    help='Directory to receive output files.')
  parser.add_option('-d', '--source_dir',
                    help='Directory containing .srpc files.',
                    default=os.path.dirname(argv[0]))
  parser.add_option('-s', '--srpcgen',
                    help='Path to the srpcgen.py script.')
  parser.add_option('-n', '--dry_run', action='store_true',
                    help='Just print commands instead of running them.',
                    default=False)
  parser.add_option('-c', '--diff_mode', action='store_true',
                    help='Touch no files, but complain if they would differ',
                    default=False)

  options, args = parser.parse_args(argv[1:])

  # Get rid of any excess ../ elements so the directory names are
  # shorter and more obvious to the eye.
  options.source_dir = os.path.normpath(options.source_dir)

  if options.output_dir is None:
    options.output_dir = options.source_dir

  if options.srpcgen is None:
    options.srpcgen = os.path.normpath(os.path.join(options.source_dir,
                                                    '..', '..', 'tools',
                                                    'srpcgen.py'))

  if args:
    parser.print_help()
    return 1

  return RunAll(options)

if __name__ == '__main__':
  sys.exit(Main(sys.argv))
