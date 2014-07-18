#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import optparse
import os
import sys
import webbrowser

from profile_chrome import chrome_controller
from profile_chrome import perf_controller
from profile_chrome import profiler
from profile_chrome import systrace_controller
from profile_chrome import ui

from pylib import android_commands
from pylib.device import device_utils


_DEFAULT_CHROME_CATEGORIES = '_DEFAULT_CHROME_CATEGORIES'


def _ComputeChromeCategories(options):
  categories = []
  if options.trace_frame_viewer:
    categories.append('disabled-by-default-cc.debug')
  if options.trace_ubercompositor:
    categories.append('disabled-by-default-cc.debug*')
  if options.trace_gpu:
    categories.append('disabled-by-default-gpu.debug*')
  if options.trace_flow:
    categories.append('disabled-by-default-toplevel.flow')
  if options.trace_memory:
    categories.append('disabled-by-default-memory')
  if options.chrome_categories:
    categories += options.chrome_categories.split(',')
  return categories


def _ComputeSystraceCategories(options):
  if not options.systrace_categories:
    return []
  return options.systrace_categories.split(',')


def _ComputePerfCategories(options):
  if not options.perf_categories:
    return []
  return options.perf_categories.split(',')


def _OptionalValueCallback(default_value):
  def callback(option, _, __, parser):
    value = default_value
    if parser.rargs and not parser.rargs[0].startswith('-'):
      value = parser.rargs.pop(0)
    setattr(parser.values, option.dest, value)
  return callback


def _CreateOptionParser():
  parser = optparse.OptionParser(description='Record about://tracing profiles '
                                 'from Android browsers. See http://dev.'
                                 'chromium.org/developers/how-tos/trace-event-'
                                 'profiling-tool for detailed instructions for '
                                 'profiling.')

  timed_options = optparse.OptionGroup(parser, 'Timed tracing')
  timed_options.add_option('-t', '--time', help='Profile for N seconds and '
                          'download the resulting trace.', metavar='N',
                           type='float')
  parser.add_option_group(timed_options)

  cont_options = optparse.OptionGroup(parser, 'Continuous tracing')
  cont_options.add_option('--continuous', help='Profile continuously until '
                          'stopped.', action='store_true')
  cont_options.add_option('--ring-buffer', help='Use the trace buffer as a '
                          'ring buffer and save its contents when stopping '
                          'instead of appending events into one long trace.',
                          action='store_true')
  parser.add_option_group(cont_options)

  chrome_opts = optparse.OptionGroup(parser, 'Chrome tracing options')
  chrome_opts.add_option('-c', '--categories', help='Select Chrome tracing '
                         'categories with comma-delimited wildcards, '
                         'e.g., "*", "cat1*,-cat1a". Omit this option to trace '
                         'Chrome\'s default categories. Chrome tracing can be '
                         'disabled with "--categories=\'\'". Use "list" to '
                         'see the available categories.',
                         metavar='CHROME_CATEGORIES', dest='chrome_categories',
                         default=_DEFAULT_CHROME_CATEGORIES)
  chrome_opts.add_option('--trace-cc',
                         help='Deprecated, use --trace-frame-viewer.',
                         action='store_true')
  chrome_opts.add_option('--trace-frame-viewer',
                         help='Enable enough trace categories for '
                         'compositor frame viewing.', action='store_true')
  chrome_opts.add_option('--trace-ubercompositor',
                         help='Enable enough trace categories for '
                         'ubercompositor frame data.', action='store_true')
  chrome_opts.add_option('--trace-gpu', help='Enable extra trace categories '
                         'for GPU data.', action='store_true')
  chrome_opts.add_option('--trace-flow', help='Enable extra trace categories '
                         'for IPC message flows.', action='store_true')
  chrome_opts.add_option('--trace-memory', help='Enable extra trace categories '
                         'for memory profile. (tcmalloc required)',
                         action='store_true')
  parser.add_option_group(chrome_opts)

  systrace_opts = optparse.OptionGroup(parser, 'Systrace tracing options')
  systrace_opts.add_option('-s', '--systrace', help='Capture a systrace with '
                        'the chosen comma-delimited systrace categories. You '
                        'can also capture a combined Chrome + systrace by '
                        'enable both types of categories. Use "list" to see '
                        'the available categories. Systrace is disabled by '
                        'default.', metavar='SYS_CATEGORIES',
                        dest='systrace_categories', default='')
  parser.add_option_group(systrace_opts)

  if perf_controller.PerfProfilerController.IsSupported():
    perf_opts = optparse.OptionGroup(parser, 'Perf profiling options')
    perf_opts.add_option('-p', '--perf', help='Capture a perf profile with '
                         'the chosen comma-delimited event categories. '
                         'Samples CPU cycles by default. Use "list" to see '
                         'the available sample types.', action='callback',
                         default='', callback=_OptionalValueCallback('cycles'),
                         metavar='PERF_CATEGORIES', dest='perf_categories')
    parser.add_option_group(perf_opts)

  output_options = optparse.OptionGroup(parser, 'Output options')
  output_options.add_option('-o', '--output', help='Save trace output to file.')
  output_options.add_option('--json', help='Save trace as raw JSON instead of '
                            'HTML.', action='store_true')
  output_options.add_option('--view', help='Open resulting trace file in a '
                            'browser.', action='store_true')
  parser.add_option_group(output_options)

  browsers = sorted(profiler.GetSupportedBrowsers().keys())
  parser.add_option('-b', '--browser', help='Select among installed browsers. '
                    'One of ' + ', '.join(browsers) + ', "stable" is used by '
                    'default.', type='choice', choices=browsers,
                    default='stable')
  parser.add_option('-v', '--verbose', help='Verbose logging.',
                    action='store_true')
  parser.add_option('-z', '--compress', help='Compress the resulting trace '
                    'with gzip. ', action='store_true')
  return parser


def main():
  parser = _CreateOptionParser()
  options, _args = parser.parse_args()
  if options.trace_cc:
    parser.parse_error("""--trace-cc is deprecated.

For basic jank busting uses, use  --trace-frame-viewer
For detailed study of ubercompositor, pass --trace-ubercompositor.

When in doubt, just try out --trace-frame-viewer.
""")

  if options.verbose:
    logging.getLogger().setLevel(logging.DEBUG)

  devices = android_commands.GetAttachedDevices()
  if len(devices) != 1:
    parser.error('Exactly 1 device must be attached.')
  device = device_utils.DeviceUtils(devices[0])
  package_info = profiler.GetSupportedBrowsers()[options.browser]

  if options.chrome_categories in ['list', 'help']:
    ui.PrintMessage('Collecting record categories list...', eol='')
    record_categories = []
    disabled_by_default_categories = []
    record_categories, disabled_by_default_categories = \
        chrome_controller.ChromeTracingController.GetCategories(
            device, package_info)

    ui.PrintMessage('done')
    ui.PrintMessage('Record Categories:')
    ui.PrintMessage('\n'.join('\t%s' % item \
        for item in sorted(record_categories)))

    ui.PrintMessage('\nDisabled by Default Categories:')
    ui.PrintMessage('\n'.join('\t%s' % item \
        for item in sorted(disabled_by_default_categories)))

    return 0

  if options.systrace_categories in ['list', 'help']:
    ui.PrintMessage('\n'.join(
        systrace_controller.SystraceController.GetCategories(device)))
    return 0

  if options.perf_categories in ['list', 'help']:
    ui.PrintMessage('\n'.join(
        perf_controller.PerfProfilerController.GetCategories(device)))
    return 0

  if not options.time and not options.continuous:
    ui.PrintMessage('Time interval or continuous tracing should be specified.')
    return 1

  chrome_categories = _ComputeChromeCategories(options)
  systrace_categories = _ComputeSystraceCategories(options)
  perf_categories = _ComputePerfCategories(options)

  if chrome_categories and 'webview' in systrace_categories:
    logging.warning('Using the "webview" category in systrace together with '
                    'Chrome tracing results in duplicate trace events.')

  enabled_controllers = []
  if chrome_categories:
    enabled_controllers.append(
        chrome_controller.ChromeTracingController(device,
                                                  package_info,
                                                  chrome_categories,
                                                  options.ring_buffer,
                                                  options.trace_memory))
  if systrace_categories:
    enabled_controllers.append(
        systrace_controller.SystraceController(device,
                                               systrace_categories,
                                               options.ring_buffer))

  if perf_categories:
    enabled_controllers.append(
        perf_controller.PerfProfilerController(device,
                                               perf_categories))

  if not enabled_controllers:
    ui.PrintMessage('No trace categories enabled.')
    return 1

  if options.output:
    options.output = os.path.expanduser(options.output)
  result = profiler.CaptureProfile(
      enabled_controllers,
      options.time if not options.continuous else 0,
      output=options.output,
      compress=options.compress,
      write_json=options.json)
  if options.view:
    if sys.platform == 'darwin':
      os.system('/usr/bin/open %s' % os.path.abspath(result))
    else:
      webbrowser.open(result)
