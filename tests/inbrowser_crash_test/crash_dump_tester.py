#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import time

script_dir = os.path.dirname(__file__)
sys.path.append(os.path.join(script_dir, '../../tools/browser_tester'))

import browser_tester

# This script extends browser_tester to check for the presence of
# Breakpad crash dumps.


# TODO(mseaborn): Change Chromium's crash_service.exe so that we can
# tell it to put dumps in a per-test temp directory.
def GetDumpDirs():
  if sys.platform == 'win32':
    # We duplicate Chromium's logic for deciding where its per-user
    # directory should be on Windows.  We can remove this if we use a
    # temp directory instead.
    import win32com.shell.shell
    import win32com.shell.shellcon
    # This typically returns a pathname like
    # 'C:\Documents and Settings\Username\Local Settings\Application Data'.
    appdata_dir = win32com.shell.shell.SHGetFolderPath(
        0, win32com.shell.shellcon.CSIDL_LOCAL_APPDATA, 0, 0)
    return [os.path.join(appdata_dir, app_name, 'User Data', 'Crash Reports')
            for app_name in ('Chromium', os.path.join('Google', 'Chrome'))]
  else:
    # TODO(mseaborn): Handle other platforms when NaCl supports
    # Breakpad crash reporting there.
    return []


def GetDumpFiles():
  file_list = []
  for dump_dir in GetDumpDirs():
    if os.path.exists(dump_dir):
      for dump_file in sorted(os.listdir(dump_dir)):
        file_list.append(os.path.join(dump_dir, dump_file))
  return file_list


def PrintDumps(desc, dump_files):
  sys.stdout.write('crash_dump_tester: Found %i %s\n' % (len(dump_files), desc))
  for dump_file in dump_files:
    sys.stdout.write('  %s\n' % dump_file)


# This reads a file of lines containing 'key:value' pairs.
# The file contains entries like the following:
#   plat:Win32
#   prod:Chromium
#   ptype:nacl-loader
#   rept:crash svc
def ReadDumpTxtFile(filename):
  dump_info = {}
  fh = open(filename, 'r')
  for line in fh:
    if ':' in line:
      key, value = line.rstrip().split(':', 1)
      dump_info[key] = value
  fh.close()
  return dump_info


def StartCrashService(browser_path):
  if sys.platform == 'win32':
    # Find crash_service.exe relative to chrome.exe.  This is a bit icky.
    browser_dir = os.path.dirname(browser_path)
    proc = subprocess.Popen([os.path.join(browser_dir, 'crash_service.exe')])
    def Cleanup():
      try:
        proc.terminate()
        sys.stdout.write('crash_dump_tester: Stopped crash_service.exe\n')
      except WindowsError:
        # If the process has already exited, we will get an 'Access is
        # denied' error.  This can happen if another instance of
        # crash_service.exe was already running, because our instance
        # will fail to claim the named pipe.
        # TODO(mseaborn): We could change crash_service.exe to create
        # unique pipe names for testing purposes.
        pass
      status = proc.wait()
      sys.stdout.write('crash_dump_tester: '
                       'crash_service.exe exited with status %s\n' % status)
    # We add a delay because there is probably a race condition:
    # crash_service.exe might not have finished doing
    # CreateNamedPipe() before NaCl does a crash dump and tries to
    # connect to that pipe.
    # TODO(mseaborn): We could change crash_service.exe to report when
    # it has successfully created the named pipe.
    time.sleep(1)
  else:
    def Cleanup():
      pass
  return Cleanup


def Main():
  parser = browser_tester.BuildArgParser()
  parser.add_option('--expected_crash_dumps', dest='expected_crash_dumps',
                    type=int, default=0,
                    help='The number of crash dumps that we should expect')
  options, args = parser.parse_args()

  # This environment variable enables Breakpad crash dumping in
  # non-official Windows builds of Chromium.
  os.environ['CHROME_HEADLESS'] = '1'

  dumps_before = GetDumpFiles()
  PrintDumps('crash dump files before the test', dumps_before)

  cleanup_func = StartCrashService(options.browser_path)
  try:
    result = browser_tester.Run(options.url, options)
  finally:
    cleanup_func()

  dumps_after = GetDumpFiles()
  PrintDumps('crash dump files after the test', dumps_after)
  # Find the new files.  This is only necessary because we are not
  # using a clean temp directory.  This is subject to a race condition
  # if running crash dump tests concurrently.
  dumps_diff = sorted(set(dumps_after).difference(dumps_before))
  PrintDumps('new crash dump files', dumps_diff)
  new_dumps = [dump_file for dump_file in dumps_diff
               if dump_file.endswith('.dmp')]

  # This produces warnings rather than errors because there are no
  # trybots for testing this, and I am not convinced that the
  # Buildbots will behave the same as my local build.
  # TODO(mseaborn): Turn these warnings into errors.
  failed = False
  msg = ('crash_dump_tester: WARNING: Got %i crash dumps but expected %i\n' %
         (len(new_dumps), options.expected_crash_dumps))
  if len(new_dumps) != options.expected_crash_dumps:
    sys.stdout.write(msg)
    failed = True
  for dump_file in new_dumps:
    # The crash dumps should come in pairs of a .dmp and .txt file.
    second_file = dump_file[:-4] + '.txt'
    msg = ('crash_dump_tester: WARNING: File %r is missing a corresponding '
           '%r file\n' % (dump_file, second_file))
    if not os.path.exists(second_file):
      sys.stdout.write(msg)
      failed = True
      continue
    # Check that the crash dump comes from the NaCl process.
    dump_info = ReadDumpTxtFile(second_file)
    if 'ptype' in dump_info:
      msg = ('crash_dump_tester: WARNING: Unexpected ptype value: %r\n'
             % dump_info['ptype'])
      if dump_info['ptype'] != 'nacl-loader':
        sys.stdout.write(msg)
        failed = True
    else:
      sys.stdout.write('crash_dump_tester: WARNING: Missing ptype field\n')
      failed = True
    # TODO(mseaborn): Ideally we would also check that a backtrace
    # containing an expected function name can be extracted from the
    # crash dump.

  if failed:
    sys.stdout.write('crash_dump_tester: FAILED, but continuing anyway\n')
  else:
    sys.stdout.write('crash_dump_tester: PASSED\n')
    # Clean up the dump files only if we are sure we produced them.
    for dump_file in dumps_diff:
      try:
        os.unlink(dump_file)
      except Exception:
        # Handle exception in case the file is locked.
        sys.stdout.write('crash_dump_tester: Deleting %r failed, '
                         'but continuing anyway\n' % dump_file)

  return result


if __name__ == '__main__':
  sys.exit(Main())
