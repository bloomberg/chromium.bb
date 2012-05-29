#!/usr/bin/env python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script fetches and prepares an SDK chroot.
"""

import os
import urlparse

from chromite.buildbot import constants
from chromite.lib import cgroups
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import locking
from chromite.lib import osutils
from chromite.lib import sudo

cros_build_lib.STRICT_SUDO = True


DEFAULT_URL = 'https://commondatastorage.googleapis.com/chromiumos-sdk'
COMPRESSION_PREFERENCE = ('xz', 'bz2')

SRC_ROOT = os.path.realpath(constants.SOURCE_ROOT)
OVERLAY_DIR = os.path.join(SRC_ROOT, 'src/third_party/chromiumos-overlay')
SDK_VERSION_FILE = os.path.join(OVERLAY_DIR,
                                'chromeos/binhost/host/sdk_version.conf')

# TODO(zbehan): Remove the dependency on these, reimplement them in python
MAKE_CHROOT = [os.path.join(SRC_ROOT, 'src/scripts/sdk_lib/make_chroot.sh')]
ENTER_CHROOT = [os.path.join(SRC_ROOT, 'src/scripts/sdk_lib/enter_chroot.sh')]

# We need these tools to run. Very common tools (tar,..) are ommited.
NEEDED_TOOLS = ('curl', 'xz')


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


def GetSdkConfig():
  """Extracts latest version from chromiumos-overlay."""
  d = {}
  with open(SDK_VERSION_FILE) as f:
    for raw_line in f:
      line = raw_line.split('#')[0].strip()
      if not line:
        continue
      chunks = line.split('=', 1)
      if len(chunks) != 2:
        raise Exception('Malformed version file; line %r' % raw_line)
      d[chunks[0]] = chunks[1].strip().strip('"')
  return d


def GetArchStageTarballs(version):
  """Returns the URL for a given arch/version"""
  extension = {'bz2':'tbz2', 'xz':'tar.xz'}
  return ['%s/cros-sdk-%s.%s'
          % (DEFAULT_URL, version, extension[compressor])
          for compressor in COMPRESSION_PREFERENCE]


def GetStage3Urls(version):
  return ['%s/stage3-amd64-%s.tar.%s' % (DEFAULT_URL, version, ext)
          for ext in COMPRESSION_PREFERENCE]


def FetchRemoteTarballs(storage_dir, urls):
  """Fetches a tarball given by url, and place it in sdk/.

  Args:
    urls: List of URLs to try to download. Download will stop on first success.

  Returns:
    Full path to the downloaded file
  """

  # Note we track content length ourselves since certain versions of curl
  # fail if asked to resume a complete file.
  # pylint: disable=C0301,W0631
  # https://sourceforge.net/tracker/?func=detail&atid=100976&aid=3482927&group_id=976
  for url in urls:
    # http://www.logilab.org/ticket/8766
    # pylint: disable=E1101
    parsed = urlparse.urlparse(url)
    tarball_name = os.path.basename(parsed.path)
    if parsed.scheme in ('', 'file'):
      if os.path.exists(parsed.path):
        return parsed.path
      continue
    content_length = 0
    print 'Attempting download: %s' % url
    result = cros_build_lib.RunCurl(
          ['-I', url], redirect_stdout=True, redirect_stderr=True,
          print_cmd=False)
    successful = False
    for header in result.output.splitlines():
      # We must walk the output to find the string '200 OK' for use cases where
      # a proxy is involved and may have pushed down the actual header.
      if header.find('200 OK') != -1:
        successful = True
      elif header.lower().startswith("content-length:"):
        content_length = int(header.split(":", 1)[-1].strip())
        if successful:
          break
    if successful:
      break
  else:
    raise Exception('No valid URLs found!')

  tarball_dest = os.path.join(storage_dir, tarball_name)
  current_size = 0
  if os.path.exists(tarball_dest):
    current_size = os.path.getsize(tarball_dest)
    if current_size > content_length:
      osutils.SafeUnlink(tarball_dest, sudo=True)
      current_size = 0

  if current_size < content_length:
    cros_build_lib.RunCurl(
        ['-f', '-L', '-y', '30', '-C', '-', '--output', tarball_dest, url],
        print_cmd=False)

  # Cleanup old tarballs now since we've successfull fetched; only cleanup
  # the tarballs for our prefix, or unknown ones.
  ignored_prefix = ('stage3-' if tarball_name.startswith('cros-sdk-')
                    else 'cros-sdk-')
  for filename in os.listdir(storage_dir):
    if filename == tarball_name or filename.startswith(ignored_prefix):
      continue

    print 'Cleaning up old tarball: %s' % (filename,)
    osutils.SafeUnlink(os.path.join(storage_dir, filename), sudo=True)

  return tarball_dest


def CreateChroot(chroot_path, sdk_tarball, cache_dir, nousepkg=False):
  """Creates a new chroot from a given SDK"""

  cmd = MAKE_CHROOT + ['--stage3_path', sdk_tarball,
                       '--chroot', chroot_path,
                       '--cache_dir', cache_dir]
  if nousepkg:
    cmd.append('--nousepkg')

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


def EnterChroot(chroot_path, cache_dir, chrome_root, chrome_root_mount,
                additional_args):
  """Enters an existing SDK chroot"""
  cmd = ENTER_CHROOT + ['--chroot', chroot_path, '--cache_dir', cache_dir]
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
--delete            .. Removes a chroot
"""
  conf = GetSdkConfig()
  sdk_latest_version = conf.get('SDK_LATEST_VERSION', '<unknown>')
  bootstrap_latest_version = conf.get('BOOTSTRAP_LATEST_VERSION', '<unknown>')

  parser = commandline.OptionParser(usage, caching=True)
  # Actions:
  parser.add_option('--bootstrap',
                    action='store_true', dest='bootstrap', default=False,
                    help=('Build everything from scratch, including the sdk.  '
                          'Use this only if you need to validate a change '
                          'that affects SDK creation itself (toolchain and '
                          'build are typically the only folk who need this).  '
                          'Note this will quite heavily slow down the build.  '
                          'Finally, this option implies --enter.'))
  parser.add_option('--delete',
                    action='store_true', dest='delete', default=False,
                    help=('Delete the current SDK chroot'))
  parser.add_option('--download',
                    action='store_true', dest='download', default=False,
                    help=('Download and install a prebuilt SDK'))
  parser.add_option('--enter',
                    action='store_true', dest='enter', default=False,
                    help=('Enter the SDK chroot, possibly (re)create first'))

  # Global options:
  default_chroot = os.path.join(SRC_ROOT, constants.DEFAULT_CHROOT_DIR)
  parser.add_option('--chroot', dest='chroot', default=default_chroot,
                    type='path',
                    help=('SDK chroot dir name [%s]' %
                          constants.DEFAULT_CHROOT_DIR))

  # Additional options:
  parser.add_option('--chrome_root',
                    dest='chrome_root', default=None, type='path',
                    help=('Mount this chrome root into the SDK chroot'))
  parser.add_option('--chrome_root_mount',
                    dest='chrome_root_mount', default=None, type='path',
                    help=('Mount chrome into this path inside SDK chroot'))
  parser.add_option('-r', '--replace',
                    action='store_true', dest='replace', default=False,
                    help=('Replace an existing SDK chroot'))
  parser.add_option('--nousepkg',
                    action='store_true', dest='nousepkg', default=False,
                    help=('Do not use binary packages when creating a chroot'))
  parser.add_option('-u', '--url',
                    dest='sdk_url', default=None,
                    help=('''Use sdk tarball located at this url.
                             Use file:// for local files.'''))
  parser.add_option('--sdk-version', default=None,
                    help='Use this sdk version.  For prebuilt, current is %r'
                         ', for bootstrapping its %r.'
                          % (sdk_latest_version, bootstrap_latest_version))
  (options, remaining_arguments) = parser.parse_args(argv)

  # Some sanity checks first, before we ask for sudo credentials.
  if cros_build_lib.IsInsideChroot():
    parser.error("This needs to be ran outside the chroot")

  host = os.uname()[4]

  if host != 'x86_64':
    parser.error(
        "cros_sdk is currently only supported on x86_64; you're running"
        " %s.  Please find a x86_64 machine." % (host,))

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

  if not options.sdk_version:
    sdk_version = (bootstrap_latest_version if options.bootstrap
                   else sdk_latest_version)
  else:
    sdk_version = options.sdk_version

  # Based on selections, fetch the tarball.
  if options.sdk_url:
    urls = [options.sdk_url]
  elif options.bootstrap:
    urls = GetStage3Urls(sdk_version)
  else:
    urls = GetArchStageTarballs(sdk_version)

  if options.delete and not os.path.exists(options.chroot):
    print "Not doing anything. The chroot you want to remove doesn't exist."
    return 0

  lock_path = os.path.dirname(options.chroot)
  lock_path = os.path.join(lock_path,
                           '.%s_lock' % os.path.basename(options.chroot))
  with sudo.SudoKeepAlive(ttyless_sudo=False):
    with cgroups.SimpleContainChildren('cros_sdk'):
      _CreateLockFile(lock_path)
      with locking.FileLock(lock_path, 'chroot lock') as lock:

        if os.path.exists(options.chroot):
          if options.delete or options.replace:
            lock.write_lock()
            DeleteChroot(options.chroot)
            if options.delete:
              return 0
          elif not options.enter and not options.download:
            print "Chroot already exists. Run with --replace to re-create."
        elif options.delete:
          return 0

        sdk_cache = os.path.join(options.cache_dir, 'sdks')
        distfiles_cache = os.path.join(options.cache_dir, 'distfiles')
        osutils.SafeMakedirs(options.cache_dir)

        for target in (sdk_cache, distfiles_cache):
          src = os.path.join(SRC_ROOT, os.path.basename(target))
          if not os.path.exists(src):
            osutils.SafeMakedirs(target)
            continue
          lock.write_lock(
              "Upgrade to %r needed but chroot is locked; please exit "
              "all instances so this upgrade can finish." % src)
          if not os.path.exists(src):
            # Note that while waiting for the write lock, src may've vanished;
            # it's a rare race during the upgrade process that's a byproduct
            # of us avoiding taking a write lock to do the src check.  If we
            # took a write lock for that check, it would effectively limit
            # all cros_sdk for a chroot to a single instance.
            osutils.SafeMakedirs(target)
          elif not os.path.exists(target):
            # Upgrade occurred, but a reversion, or something whacky
            # occurred writing to the old location.  Wipe and continue.
            cros_build_lib.SudoRunCommand(
                ['mv', '--', src, target], print_cmd=False)
          else:
            # Upgrade occurred once already, but either a reversion or
            # some before/after separate cros_sdk usage is at play.
            # Wipe and continue.
            osutils.RmDir(src, sudo=True)

        if not os.path.exists(options.chroot) or options.download:
          lock.write_lock()
          sdk_tarball = FetchRemoteTarballs(sdk_cache, urls)
          if options.download:
            # Nothing further to do.
            return 0
          CreateChroot(options.chroot, sdk_tarball, options.cache_dir,
                       nousepkg=(options.bootstrap or options.nousepkg))

        if options.enter:
          lock.read_lock()
          EnterChroot(options.chroot, options.cache_dir, options.chrome_root,
                      options.chrome_root_mount, remaining_arguments)
