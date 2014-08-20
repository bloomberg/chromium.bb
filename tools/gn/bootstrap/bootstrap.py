#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Bootstraps gn.

It is done by first building it manually in a temporary directory, then building
it with its own BUILD.gn to the final destination.
"""

import contextlib
import errno
import logging
import optparse
import os
import shutil
import subprocess
import sys
import tempfile

BOOTSTRAP_DIR = os.path.dirname(os.path.abspath(__file__))
GN_ROOT = os.path.dirname(BOOTSTRAP_DIR)
SRC_ROOT = os.path.dirname(os.path.dirname(GN_ROOT))

is_linux = sys.platform.startswith('linux')
is_mac = sys.platform.startswith('darwin')
is_posix = is_linux or is_mac

def check_call(cmd, **kwargs):
  logging.debug('Running: %s', ' '.join(cmd))
  subprocess.check_call(cmd, cwd=GN_ROOT, **kwargs)

def mkdir_p(path):
  try:
    os.makedirs(path)
  except OSError as e:
    if e.errno == errno.EEXIST and os.path.isdir(path):
      pass
    else: raise

@contextlib.contextmanager
def scoped_tempdir():
  path = tempfile.mkdtemp()
  try:
    yield path
  finally:
    shutil.rmtree(path)


def main(argv):
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option('-d', '--debug', action='store_true',
                    help='Do a debug build. Defaults to release build.')
  parser.add_option('-o', '--output',
                    help='place output in PATH', metavar='PATH')
  parser.add_option('-s', '--no-rebuild', action='store_true',
                    help='Do not rebuild GN with GN.')
  parser.add_option('-v', '--verbose', action='store_true',
                    help='Log more details')
  options, args = parser.parse_args(argv)

  if args:
    parser.error('Unrecognized command line arguments: %s.' % ', '.join(args))

  logging.basicConfig(level=logging.DEBUG if options.verbose else logging.ERROR)

  if options.debug:
    build_rel = os.path.join('out', 'Debug')
  else:
    build_rel = os.path.join('out', 'Release')
  build_root = os.path.join(SRC_ROOT, build_rel)

  try:
    with scoped_tempdir() as tempdir:
      print 'Building gn manually in a temporary directory for bootstrapping...'
      build_gn_with_ninja_manually(tempdir, options)
      temp_gn = os.path.join(tempdir, 'gn')
      out_gn = os.path.join(build_root, 'gn')

      if options.no_rebuild:
        mkdir_p(build_root)
        shutil.copy2(temp_gn, out_gn)
      else:
        print 'Building gn using itself to %s...' % build_rel
        build_gn_with_gn(temp_gn, build_rel, options)

      if options.output:
        # Preserve the executable permission bit.
        shutil.copy2(out_gn, options.output)
  except subprocess.CalledProcessError as e:
    print >> sys.stderr, str(e)
    return 1
  return 0


def build_gn_with_ninja_manually(tempdir, options):
  write_ninja(os.path.join(tempdir, 'build.ninja'), options)
  cmd = ['ninja', '-C', tempdir]
  if options.verbose:
    cmd.append('-v')
  cmd.append('gn')
  check_call(cmd)

def write_ninja(path, options):
  cc = os.environ.get('CC', '')
  cxx = os.environ.get('CXX', '')
  cflags = os.environ.get('CFLAGS', '').split()
  cflags_cc = os.environ.get('CXXFLAGS', '').split()
  ld = os.environ.get('LD', cxx)
  ldflags = os.environ.get('LDFLAGS', '').split()
  include_dirs = [SRC_ROOT]
  libs = []

  if is_posix:
    if options.debug:
      cflags.extend(['-O0', '-g'])
    else:
      cflags.extend(['-O2', '-g0'])

    cflags.extend(['-D_FILE_OFFSET_BITS=64', '-pthread', '-pipe'])
    cflags_cc.extend(['-std=gnu++11', '-Wno-c++11-narrowing'])

  static_libraries = {
      'base': {'sources': [], 'tool': 'cxx'},
      'dynamic_annotations': {'sources': [], 'tool': 'cc'},
      'gn': {'sources': [], 'tool': 'cxx'},
  }

  for name in os.listdir(GN_ROOT):
    if not name.endswith('.cc'):
      continue
    if name.endswith('_unittest.cc'):
      continue
    if name in ['generate_test_gn_data.cc', 'run_all_unittests.cc']:
      continue
    full_path = os.path.join(GN_ROOT, name)
    static_libraries['gn']['sources'].append(
        os.path.relpath(full_path, SRC_ROOT))

  static_libraries['dynamic_annotations']['sources'].extend([
      'base/third_party/dynamic_annotations/dynamic_annotations.c',
  ])
  static_libraries['base']['sources'].extend([
      'base/at_exit.cc',
      'base/atomicops_internals_x86_gcc.cc',
      'base/base_paths.cc',
      'base/base_switches.cc',
      'base/callback_internal.cc',
      'base/command_line.cc',
      'base/debug/alias.cc',
      'base/debug/stack_trace.cc',
      'base/debug/task_annotator.cc',
      'base/debug/trace_event_impl.cc',
      'base/debug/trace_event_impl_constants.cc',
      'base/debug/trace_event_memory.cc',
      'base/debug/trace_event_synthetic_delay.cc',
      'base/environment.cc',
      'base/files/file.cc',
      'base/files/file_enumerator.cc',
      'base/files/file_path.cc',
      'base/files/file_path_constants.cc',
      'base/files/file_util.cc',
      'base/files/scoped_file.cc',
      'base/json/json_parser.cc',
      'base/json/json_reader.cc',
      'base/json/json_string_value_serializer.cc',
      'base/json/json_writer.cc',
      'base/json/string_escape.cc',
      'base/lazy_instance.cc',
      'base/location.cc',
      'base/logging.cc',
      'base/memory/ref_counted.cc',
      'base/memory/ref_counted_memory.cc',
      'base/memory/singleton.cc',
      'base/memory/weak_ptr.cc',
      'base/message_loop/incoming_task_queue.cc',
      'base/message_loop/message_loop.cc',
      'base/message_loop/message_loop_proxy.cc',
      'base/message_loop/message_loop_proxy_impl.cc',
      'base/message_loop/message_pump.cc',
      'base/message_loop/message_pump_default.cc',
      'base/metrics/bucket_ranges.cc',
      'base/metrics/histogram.cc',
      'base/metrics/histogram_base.cc',
      'base/metrics/histogram_samples.cc',
      'base/metrics/sample_map.cc',
      'base/metrics/sample_vector.cc',
      'base/metrics/sparse_histogram.cc',
      'base/metrics/statistics_recorder.cc',
      'base/path_service.cc',
      'base/pending_task.cc',
      'base/pickle.cc',
      'base/process/kill.cc',
      'base/process/process_iterator.cc',
      'base/process/process_metrics.cc',
      'base/profiler/alternate_timer.cc',
      'base/profiler/tracked_time.cc',
      'base/run_loop.cc',
      'base/sequence_checker_impl.cc',
      'base/sequenced_task_runner.cc',
      'base/strings/string16.cc',
      'base/strings/string_number_conversions.cc',
      'base/strings/string_piece.cc',
      'base/strings/string_split.cc',
      'base/strings/string_util.cc',
      'base/strings/string_util_constants.cc',
      'base/strings/stringprintf.cc',
      'base/strings/utf_string_conversion_utils.cc',
      'base/strings/utf_string_conversions.cc',
      'base/synchronization/cancellation_flag.cc',
      'base/synchronization/lock.cc',
      'base/sys_info.cc',
      'base/task_runner.cc',
      'base/third_party/dmg_fp/dtoa_wrapper.cc',
      'base/third_party/dmg_fp/g_fmt.cc',
      'base/third_party/icu/icu_utf.cc',
      'base/third_party/nspr/prtime.cc',
      'base/thread_task_runner_handle.cc',
      'base/threading/non_thread_safe_impl.cc',
      'base/threading/post_task_and_reply_impl.cc',
      'base/threading/sequenced_worker_pool.cc',
      'base/threading/simple_thread.cc',
      'base/threading/thread_checker_impl.cc',
      'base/threading/thread_collision_warner.cc',
      'base/threading/thread_id_name_manager.cc',
      'base/threading/thread_local_storage.cc',
      'base/threading/thread_restrictions.cc',
      'base/time/time.cc',
      'base/timer/elapsed_timer.cc',
      'base/timer/timer.cc',
      'base/tracked_objects.cc',
      'base/tracking_info.cc',
      'base/values.cc',
      'base/vlog.cc',
  ])

  if is_posix:
    static_libraries['base']['sources'].extend([
        'base/base_paths_posix.cc',
        'base/debug/debugger_posix.cc',
        'base/debug/stack_trace_posix.cc',
        'base/files/file_enumerator_posix.cc',
        'base/files/file_posix.cc',
        'base/files/file_util_posix.cc',
        'base/message_loop/message_pump_libevent.cc',
        'base/posix/file_descriptor_shuffle.cc',
        'base/process/kill_posix.cc',
        'base/process/process_handle_posix.cc',
        'base/process/process_metrics_posix.cc',
        'base/process/process_posix.cc',
        'base/safe_strerror_posix.cc',
        'base/synchronization/condition_variable_posix.cc',
        'base/synchronization/lock_impl_posix.cc',
        'base/synchronization/waitable_event_posix.cc',
        'base/sys_info_posix.cc',
        'base/threading/platform_thread_posix.cc',
        'base/threading/thread_local_posix.cc',
        'base/threading/thread_local_storage_posix.cc',
        'base/time/time_posix.cc',
    ])
    static_libraries['libevent'] = {
        'sources': [
            'third_party/libevent/buffer.c',
            'third_party/libevent/evbuffer.c',
            'third_party/libevent/evdns.c',
            'third_party/libevent/event.c',
            'third_party/libevent/event_tagging.c',
            'third_party/libevent/evrpc.c',
            'third_party/libevent/evutil.c',
            'third_party/libevent/http.c',
            'third_party/libevent/log.c',
            'third_party/libevent/poll.c',
            'third_party/libevent/select.c',
            'third_party/libevent/signal.c',
            'third_party/libevent/strlcpy.c',
        ],
        'tool': 'cc',
        'include_dirs': [],
        'cflags': cflags + ['-DHAVE_CONFIG_H'],
    }


  if is_linux:
    libs.extend(['-lrt'])
    ldflags.extend(['-pthread'])

    static_libraries['xdg_user_dirs'] = {
        'sources': [
            'base/third_party/xdg_user_dirs/xdg_user_dir_lookup.cc',
        ],
        'tool': 'cxx',
    }
    static_libraries['base']['sources'].extend([
        'base/nix/xdg_util.cc',
        'base/process/internal_linux.cc',
        'base/process/process_handle_linux.cc',
        'base/process/process_iterator_linux.cc',
        'base/process/process_linux.cc',
        'base/process/process_metrics_linux.cc',
        'base/strings/sys_string_conversions_posix.cc',
        'base/sys_info_linux.cc',
        'base/threading/platform_thread_linux.cc',
    ])
    static_libraries['libevent']['include_dirs'].extend([
        os.path.join(SRC_ROOT, 'third_party', 'libevent', 'linux')
    ])
    static_libraries['libevent']['sources'].extend([
        'third_party/libevent/epoll.c',
    ])


  if is_mac:
    static_libraries['base']['sources'].extend([
        'base/base_paths_mac.mm',
        'base/files/file_util_mac.mm',
        'base/mac/bundle_locations.mm',
        'base/mac/foundation_util.mm',
        'base/mac/mach_logging.cc',
        'base/mac/scoped_mach_port.cc',
        'base/mac/scoped_nsautorelease_pool.mm',
        'base/message_loop/message_pump_mac.mm',
        'base/process/process_handle_mac.cc',
        'base/process/process_iterator_mac.cc',
        'base/strings/sys_string_conversions_mac.mm',
        'base/time/time_mac.cc',
        'base/threading/platform_thread_mac.mm',
    ])
    static_libraries['libevent']['include_dirs'].extend([
        os.path.join(SRC_ROOT, 'third_party', 'libevent', 'mac')
    ])
    static_libraries['libevent']['sources'].extend([
        'third_party/libevent/kqueue.c',
    ])


  if is_mac:
    template_filename = 'build_mac.ninja.template'
  else:
    template_filename = 'build.ninja.template'

  with open(os.path.join(GN_ROOT, 'bootstrap', template_filename)) as f:
    ninja_template = f.read()

  def src_to_obj(path):
    return '%s' % os.path.splitext(path)[0] + '.o'

  ninja_lines = []
  for library, settings in static_libraries.iteritems():
    for src_file in settings['sources']:
      ninja_lines.extend([
          'build %s: %s %s' % (src_to_obj(src_file),
                               settings['tool'],
                               os.path.join(SRC_ROOT, src_file)),
          '  includes = %s' % ' '.join(
              ['-I' + dirname for dirname in
               include_dirs + settings.get('include_dirs', [])]),
          '  cflags = %s' % ' '.join(cflags + settings.get('cflags', [])),
          '  cflags_cc = %s' %
              ' '.join(cflags_cc + settings.get('cflags_cc', [])),
      ])
      if cc:
        ninja_lines.append('  cc = %s' % cc)
      if cxx:
        ninja_lines.append('  cxx = %s' % cxx)

    ninja_lines.append('build %s.a: alink_thin %s' % (
        library,
        ' '.join([src_to_obj(src_file) for src_file in settings['sources']])))

  if is_mac:
    libs.extend([
        '-framework', 'AppKit',
        '-framework', 'CoreFoundation',
        '-framework', 'Foundation',
        '-framework', 'Security',
    ]);

  ninja_lines.extend([
      'build gn: link %s' % (
          ' '.join(['%s.a' % library for library in static_libraries])),
      '  ldflags = %s' % ' '.join(ldflags),
      '  libs = %s' % ' '.join(libs),
  ])
  if ld:
    ninja_lines.append('  ld = %s' % ld)
  else:
    ninja_lines.append('  ld = $ldxx')

  ninja_lines.append('')  # Make sure the file ends with a newline.

  with open(path, 'w') as f:
    f.write(ninja_template + '\n'.join(ninja_lines))


def build_gn_with_gn(temp_gn, build_dir, options):
  cmd = [temp_gn, 'gen', build_dir]
  if not options.debug:
    cmd.append('--args=is_debug=false')
  check_call(cmd)

  cmd = ['ninja', '-C', build_dir]
  if options.verbose:
    cmd.append('-v')
  cmd.append('gn')
  check_call(cmd)

  if not debug:
    check_call(['strip', os.path.join(build_dir, 'gn')])


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
