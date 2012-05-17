#!/usr/bin/env python
# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script fetches and prepares an SDK chroot.
"""

import optparse
import os
import sys
import urlparse

from chromite.buildbot import constants
from chromite.lib import cgroups
from chromite.lib import cros_build_lib
from chromite.lib import locking
from chromite.lib import sudo

cros_build_lib.STRICT_SUDO = True


DEFAULT_URL = 'https://commondatastorage.googleapis.com/chromiumos-sdk/'
SDK_SUFFIXES = ['.tbz2', '.tar.xz']

SRC_ROOT = os.path.realpath(constants.SOURCE_ROOT)
SDK_DIR = os.path.join(SRC_ROOT, 'sdks')
OVERLAY_DIR = os.path.join(SRC_ROOT, 'src/third_party/chromiumos-overlay')
SDK_VERSION_FILE = os.path.join(OVERLAY_DIR,
                                'chromeos/binhost/host/sdk_version.conf')

# TODO(zbehan): Remove the dependency on these, reimplement them in python
MAKE_CHROOT = [os.path.join(SRC_ROOT, 'src/scripts/sdk_lib/make_chroot.sh')]
ENTER_CHROOT = [os.path.join(SRC_ROOT, 'src/scripts/sdk_lib/enter_chroot.sh')]

# We need these tools to run. Very common tools (tar,..) are ommited.
NEEDED_TOOLS = ['curl']

def GetHostArch():
  """Returns a string for the host architecture"""
  out = cros_build_lib.RunCommand(['uname', '-m'],
      redirect_stdout=True, print_cmd=False).output
  return out.rstrip('\n')

def CheckPrerequisites(needed_tools):
  """Verifies that the required tools are present on the system.

  This is especially important as this script is intended to run
  outside the chroot.

  Arguments:
    needed_tools: an array of string specified binaries to look for.

  Returns:
    True if all needed tools were found.
  """
  missing = []
  for tool in needed_tools:
    cmd = ['which', tool]
    try:
      cros_build_lib.RunCommand(cmd, print_cmd=False, redirect_stdout=True,
                                combine_stdout_stderr=True)
    except cros_build_lib.RunCommandError:
      missing.append(tool)
  return missing


def GetLatestVersion():
  """Extracts latest version from chromiumos-overlay."""
  sdk_file = open(SDK_VERSION_FILE)
  buf = sdk_file.readline().rstrip('\n').split('=')
  if buf[0] != 'SDK_LATEST_VERSION':
    raise Exception('Malformed version file')
  return buf[1].strip('"')


def GetArchStageTarballs(tarballArch, version):
  """Returns the URL for a given arch/version"""
  D = { 'x86_64': 'cros-sdk-' }
  try:
    return [DEFAULT_URL + D[tarballArch] + version + x for x in SDK_SUFFIXES]
  except KeyError:
    raise SystemExit('Unsupported arch: %s' % (tarballArch,))


def FetchRemoteTarballs(urls):
  """Fetches a tarball given by url, and place it in sdk/.

  Args:
    urls: List of URLs to try to download. Download will stop on first success.

  Returns:
    Full path to the downloaded file
  """

  def RunCurl(args, **kwargs):
    """Runs curl and wraps around all necessary hacks."""
    cmd = ['curl']
    cmd.extend(args)

    # These values were discerned via scraping the curl manpage; they're all
    # retry related (dns failed, timeout occurred, etc, see  the manpage for
    # exact specifics of each).
    # Note we allow 22 to deal w/ 500's- they're thrown by google storage
    # occasionally.
    # Finally, we do not use curl's --retry option since it generally doesn't
    # actually retry anything; code 18 for example, it will not retry on.
    retriable_exits = frozenset([5, 6, 7, 15, 18, 22, 26, 28, 52])
    try:
      return cros_build_lib.RunCommandWithRetries(
          5, cmd, sleep=3, retry_on=retriable_exits, **kwargs)
    except cros_build_lib.RunCommandError, e:
      code = e.result.returncode
      if e.returncode in (51, 58, 60):
        # These are the return codes of failing certs as per 'man curl'.
        print 'Download failed with certificate error? Try "sudo c_rehash".'
      else:
        print "Curl failed w/ exit code %i" % code
      sys.exit(1)

  def RemoteTarballExists(url):
    """Tests if a remote tarball exists."""
    # We also use this for "local" tarballs using file:// urls. Those will
    # fail the -I check, so just check the file locally instead.
    if url.startswith('file://'):
      return os.path.exists(url.replace('file://', ''))

    result = RunCurl(['-I', url],
                     redirect_stdout=True, redirect_stderr=True,
                     print_cmd=False)
    header = result.output.splitlines()[0]
    return header.find('200 OK') != -1

  url = None
  for url in urls:
    print 'Attempting download: %s' % url
    if RemoteTarballExists(url):
      break
  else:
    raise Exception('No valid URLs found!')

  # pylint: disable=E1101
  tarball_name = os.path.basename(urlparse.urlparse(url).path)
  tarball_dest = os.path.join(SDK_DIR, tarball_name)

  # Cleanup old tarballs.
  files_to_delete = [f for f in os.listdir(SDK_DIR) if f != tarball_name]
  if files_to_delete:
    print 'Cleaning up old tarballs: ' + str(files_to_delete)
    for f in files_to_delete:
      f_path = os.path.join(SDK_DIR, f)
      # Only delete regular files that belong to us.
      if os.path.isfile(f_path) and os.stat(f_path).st_uid == os.getuid():
        os.remove(f_path)

  curl_opts = ['-f', '-L', '-y', '30', '--output', tarball_dest]
  if not url.startswith('file://') and os.path.exists(tarball_dest):
    # Only resume for remote URLs. If the file is local, there's no
    # real speedup, and using the same filename for different files
    # locally will cause issues.
    curl_opts.extend(['-C', '-'])

    # Additionally, certain versions of curl incorrectly fail if
    # told to resume a file that is fully downloaded, thus do a
    # check on our own.
    # see:
    # pylint: disable=C0301
    # https://sourceforge.net/tracker/?func=detail&atid=100976&aid=3482927&group_id=976
    result = RunCurl(['-I', url],
                     redirect_stdout=True,
                     redirect_stderr=True,
                     print_cmd=False)

    for x in result.output.splitlines():
      if x.lower().startswith("content-length:"):
        length = int(x.split(":", 1)[-1].strip())
        if length == os.path.getsize(tarball_dest):
          # Fully fetched; bypass invoking curl, since it can screw up handling
          # of this (>=7.21.4 and up).
          return tarball_dest
        break
  curl_opts.append(url)
  RunCurl(curl_opts)
  return tarball_dest


def BootstrapChroot(chroot_path, stage_url, replace):
  """Builds a new chroot from source"""
  cmd = MAKE_CHROOT + ['--chroot', chroot_path,
                       '--nousepkg']

  stage = None
  if stage_url:
    stage = FetchRemoteTarballs([stage_url])

  if stage:
    cmd.extend(['--stage3_path', stage])

  if replace:
    cmd.append('--replace')

  try:
    cros_build_lib.RunCommand(cmd, print_cmd=False)
  except cros_build_lib.RunCommandError:
    raise SystemExit('Running %r failed!' % cmd)


def CreateChroot(sdk_url, sdk_version, chroot_path, replace):
  """Creates a new chroot from a given SDK"""
  if not os.path.exists(SDK_DIR):
    cros_build_lib.RunCommand(['mkdir', '-p', SDK_DIR], print_cmd=False)

  # Based on selections, fetch the tarball
  if sdk_url:
    urls = [sdk_url]
  else:
    arch = GetHostArch()
    urls = GetArchStageTarballs(arch, sdk_version)

  sdk = FetchRemoteTarballs(urls)

  # TODO(zbehan): Unpack and install
  # For now, we simply call make_chroot on the prebuilt chromeos-sdk.
  # make_chroot provides a variety of hacks to make the chroot useable.
  # These should all be eliminated/minimised, after which, we can change
  # this to just unpacking the sdk.
  cmd = MAKE_CHROOT + ['--stage3_path', sdk,
                       '--chroot', chroot_path]

  if replace:
    cmd.append('--replace')

  try:
    cros_build_lib.RunCommand(cmd, print_cmd=False)
  except cros_build_lib.RunCommandError:
    raise SystemExit('Running %r failed!' % cmd)


def DeleteChroot(chroot_path):
  """Deletes an existing chroot"""
  cmd = MAKE_CHROOT + ['--chroot', chroot_path,
                       '--delete']
  try:
    cros_build_lib.RunCommand(cmd, print_cmd=False)
  except cros_build_lib.RunCommandError:
    raise SystemExit('Running %r failed!' % cmd)


def _CreateLockFile(path):
  """Create a lockfile via sudo that is writable by current user."""
  cros_build_lib.SudoRunCommand(['touch', path], print_cmd=False)
  cros_build_lib.SudoRunCommand(['chown', str(os.getuid()), path],
                                print_cmd=False)
  cros_build_lib.SudoRunCommand(['chmod', '644', path], print_cmd=False)


def EnterChroot(chroot_path, chrome_root, chrome_root_mount, additional_args):
  """Enters an existing SDK chroot"""
  cmd = ENTER_CHROOT + ['--chroot', chroot_path]
  if chrome_root:
    cmd.extend(['--chrome_root', chrome_root])
  if chrome_root_mount:
    cmd.extend(['--chrome_root_mount', chrome_root_mount])
  if len(additional_args) > 0:
    cmd.append('--')
    cmd.extend(additional_args)

  ret = cros_build_lib.RunCommand(cmd, print_cmd=False, error_code_ok=True)
  # If we were in interactive mode, ignore the exit code; it'll be whatever
  # they last ran w/in the chroot and won't matter to us one way or another.
  # Note this does allow chroot entrance to fail and be ignored during
  # interactive; this is however a rare case and the user will immediately
  # see it (nor will they be checking the exit code manually).
  if ret.returncode != 0 and additional_args:
    raise SystemExit('Running %r failed with exit code %i'
                     % (cmd, ret.returncode))


def main(argv):
  # TODO(ferringb): make argv required once depot_tools is fixed.
  usage = """usage: %prog [options] [VAR1=val1 .. VARn=valn -- <args>]

This script manages a local CrOS SDK chroot. Depending on the flags,
it can download, build or enter a chroot.

Action taken is the following:
--enter  (default)  .. Installs and enters a chroot
--download          .. Just download a chroot (enter if combined with --enter)
--bootstrap         .. Builds a chroot from source (enter if --enter)
--delete            .. Removes a chroot
"""
  sdk_latest_version = GetLatestVersion()
  parser = optparse.OptionParser(usage)
  # Actions:
  parser.add_option('', '--bootstrap',
                    action='store_true', dest='bootstrap', default=False,
                    help=('Build a new SDK chroot from source'))
  parser.add_option('', '--delete',
                    action='store_true', dest='delete', default=False,
                    help=('Delete the current SDK chroot'))
  parser.add_option('', '--download',
                    action='store_true', dest='download', default=False,
                    help=('Download and install a prebuilt SDK'))
  parser.add_option('', '--enter',
                    action='store_true', dest='enter', default=False,
                    help=('Enter the SDK chroot, possibly (re)create first'))

  # Global options:
  parser.add_option('', '--chroot',
                    dest='chroot', default=constants.DEFAULT_CHROOT_DIR,
                    help=('SDK chroot dir name [%s]' %
                          constants.DEFAULT_CHROOT_DIR))

  # Additional options:
  parser.add_option('', '--chrome_root',
                    dest='chrome_root', default='',
                    help=('Mount this chrome root into the SDK chroot'))
  parser.add_option('', '--chrome_root_mount',
                    dest='chrome_root_mount', default='',
                    help=('Mount chrome into this path inside SDK chroot'))
  parser.add_option('-r', '--replace',
                    action='store_true', dest='replace', default=False,
                    help=('Replace an existing SDK chroot'))
  parser.add_option('-u', '--url',
                    dest='sdk_url', default='',
                    help=('''Use sdk tarball located at this url.
                             Use file:// for local files.'''))
  parser.add_option('-v', '--version',
                    dest='sdk_version', default='',
                    help=('Use this sdk version [%s]' % sdk_latest_version))
  (options, remaining_arguments) = parser.parse_args(argv)

  # Some sanity checks first, before we ask for sudo credentials.
  if cros_build_lib.IsInsideChroot():
    parser.error("This needs to be ran outside the chroot")

  missing = CheckPrerequisites(NEEDED_TOOLS)
  if missing:
    parser.error((
        'The tool(s) %s were not found.'
        'Please install the appropriate package in your host.'
        'Example(ubuntu):'
        '  sudo apt-get install <packagename>'
        % (', '.join(missing))))

  # Default action is --enter, if no other is selected.
  if not (options.bootstrap or options.download or options.delete):
    options.enter = True

  # Only --enter can process additional args as passthrough commands.
  # Warn and exit for least surprise.
  if len(remaining_arguments) > 0 and not options.enter:
    parser.error("Additional arguments are not permitted, unless running "
                 "with --enter")

  # Some actions can be combined, as they merely modify how is the chroot
  # going to be made. The only option that hates all others is --delete.
  if options.delete and \
    (options.enter or options.download or options.bootstrap):
    parser.error("--delete cannot be combined with --enter, "
                 "--download or --bootstrap")
  # NOTE: --delete is a true hater, it doesn't like other options either, but
  # those will hardly lead to confusion. Nobody can expect to pass --version to
  # delete and actually change something.

  if options.bootstrap and options.download:
    parser.error("Either --bootstrap or --download, not both")

  # Bootstrap will start off from a non-selectable stage3 tarball. Attempts to
  # select sdk by version are confusing. Warn and exit. We can still specify a
  # tarball by path or URL though.
  if options.bootstrap and options.sdk_version:
    parser.error("Cannot use --version when bootstrapping")

  chroot_path = os.path.join(SRC_ROOT, options.chroot)
  chroot_path = os.path.abspath(chroot_path)
  chroot_path = os.path.normpath(chroot_path)

  if not options.sdk_version:
    sdk_version = sdk_latest_version
  else:
    sdk_version = options.sdk_version

  if options.delete and not os.path.exists(chroot_path):
    print "Not doing anything. The chroot you want to remove doesn't exist."
    return 0

  lock_path = os.path.dirname(chroot_path)
  lock_path = os.path.join(lock_path,
                           '.%s_lock' % os.path.basename(chroot_path))
  with sudo.SudoKeepAlive():
    with cgroups.SimpleContainChildren('cros_sdk'):
      _CreateLockFile(lock_path)
      with locking.FileLock(lock_path, 'chroot lock') as lock:
        if options.delete:
          lock.write_lock()
          DeleteChroot(chroot_path)
          return 0

        # Print a suggestion for replacement, but not if running just --enter.
        if os.path.exists(chroot_path) and not options.replace and \
            (options.bootstrap or options.download):
          print "Chroot already exists. Run with --replace to re-create."

        # Chroot doesn't exist or asked to replace.
        if not os.path.exists(chroot_path) or options.replace:
          lock.write_lock()
          if options.bootstrap:
            BootstrapChroot(chroot_path, options.sdk_url,
                            options.replace)
          else:
            CreateChroot(options.sdk_url, sdk_version,
                         chroot_path, options.replace)
        if options.enter:
          lock.read_lock()
          EnterChroot(chroot_path, options.chrome_root,
                      options.chrome_root_mount, remaining_arguments)
