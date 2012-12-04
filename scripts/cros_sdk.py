#!/usr/bin/env python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script fetches and prepares an SDK chroot.
"""

import errno
import os
import sys
import urlparse

from chromite.buildbot import constants
from chromite.lib import cgroups
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import locking
from chromite.lib import osutils

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
NEEDED_TOOLS = ('curl', 'xz', 'unshare')


def GetSdkConfig():
  """Extracts latest version from chromiumos-overlay."""
  d = {}
  try:
    with open(SDK_VERSION_FILE) as f:
      for raw_line in f:
        line = raw_line.split('#')[0].strip()
        if not line:
          continue
        chunks = line.split('=', 1)
        if len(chunks) != 2:
          raise Exception('Malformed version file; line %r' % raw_line)
        d[chunks[0]] = chunks[1].strip().strip('"')
  except EnvironmentError, e:
    if e.errno != errno.ENOENT:
      raise
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
      osutils.SafeUnlink(tarball_dest)
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
    osutils.SafeUnlink(os.path.join(storage_dir, filename))

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


def _SudoCommand():
  """Get the 'sudo' command, along with all needed environment variables."""

  # Pass in the ENVIRONMENT_WHITELIST variable so that scripts in the chroot
  # know what variables to pass through.
  cmd = ['sudo']
  for key in constants.CHROOT_ENVIRONMENT_WHITELIST:
    value = os.environ.get(key)
    if value is not None:
      cmd += ['%s=%s' % (key, value)]

  # Pass in the path to the depot_tools so that users can access them from
  # within the chroot.
  gclient = osutils.Which('gclient')
  if gclient is not None:
    cmd += ['DEPOT_TOOLS=%s' % os.path.realpath(os.path.dirname(gclient))]

  return cmd


def _ReExecuteIfNeeded(argv):
  """Re-execute cros_sdk as root.

  Also unshare the mount namespace so as to ensure that processes outside
  the chroot can't mess with our mounts.
  """
  MAGIC_VAR = '%CROS_SDK_MOUNT_NS'
  if os.geteuid() != 0:
    cmd = _SudoCommand() + ['--'] + argv
    os.execvp(cmd[0], cmd)
  elif os.environ.get(MAGIC_VAR, '0') == '0':
    cgroups.Cgroup.InitSystem()
    os.environ[MAGIC_VAR] = '1'
    os.execvp('unshare', ['unshare', '-m', '--'] + argv)
  else:
    os.environ.pop(MAGIC_VAR)


def main(argv):
  usage = """usage: %prog [options] [VAR1=val1 .. VARn=valn -- args]

This script is used for manipulating local chroot environments; creating,
deleting, downloading, etc.  If given --enter (or no args), it defaults
to an interactive bash shell within the chroot.

If given args those are passed to the chroot environment, and executed."""
  conf = GetSdkConfig()
  sdk_latest_version = conf.get('SDK_LATEST_VERSION', '<unknown>')
  bootstrap_latest_version = conf.get('BOOTSTRAP_LATEST_VERSION', '<unknown>')

  parser = commandline.OptionParser(usage=usage, caching=True)

  commands = parser.add_option_group("Commands")
  commands.add_option(
      '--enter', action='store_true', default=False,
      help='Enter the SDK chroot.  Implies --create.')
  commands.add_option(
      '--create', action='store_true',default=False,
      help='Create the chroot only if it does not already exist.  '
      'Implies --download.')
  commands.add_option(
      '--bootstrap', action='store_true', default=False,
      help='Build everything from scratch, including the sdk.  '
      'Use this only if you need to validate a change '
      'that affects SDK creation itself (toolchain and '
      'build are typically the only folk who need this).  '
      'Note this will quite heavily slow down the build.  '
      'This option implies --create --nousepkg.')
  commands.add_option(
      '-r', '--replace', action='store_true', default=False,
      help='Replace an existing SDK chroot.  Basically an alias '
      'for --delete --create.')
  commands.add_option(
      '--delete', action='store_true', default=False,
      help='Delete the current SDK chroot if it exists.')
  commands.add_option(
      '--download', action='store_true', default=False,
      help='Download the sdk.')

  # Global options:
  default_chroot = os.path.join(SRC_ROOT, constants.DEFAULT_CHROOT_DIR)
  parser.add_option(
      '--chroot', dest='chroot', default=default_chroot, type='path',
      help=('SDK chroot dir name [%s]' % constants.DEFAULT_CHROOT_DIR))

  parser.add_option('--chrome_root', default=None, type='path',
                    help='Mount this chrome root into the SDK chroot')
  parser.add_option('--chrome_root_mount', default=None, type='path',
                    help='Mount chrome into this path inside SDK chroot')
  parser.add_option('--nousepkg', action='store_true', default=False,
                    help='Do not use binary packages when creating a chroot.')
  parser.add_option('-u', '--url',
                    dest='sdk_url', default=None,
                    help=('''Use sdk tarball located at this url.
                             Use file:// for local files.'''))
  parser.add_option('--sdk-version', default=None,
                    help='Use this sdk version.  For prebuilt, current is %r'
                         ', for bootstrapping its %r.'
                          % (sdk_latest_version, bootstrap_latest_version))
  options, chroot_command = parser.parse_args(argv)

  # Some sanity checks first, before we ask for sudo credentials.
  cros_build_lib.AssertOutsideChroot()

  _ReExecuteIfNeeded([sys.argv[0]] + argv)

  host = os.uname()[4]
  if host != 'x86_64':
    parser.error(
        "cros_sdk is currently only supported on x86_64; you're running"
        " %s.  Please find a x86_64 machine." % (host,))

  missing = osutils.FindMissingBinaries(NEEDED_TOOLS)
  if missing:
    parser.error((
        'The tool(s) %s were not found.'
        'Please install the appropriate package in your host.'
        'Example(ubuntu):'
        '  sudo apt-get install <packagename>'
        % (', '.join(missing))))

  # Expand out the aliases...
  if options.replace:
    options.delete = options.create = True

  if options.bootstrap:
    options.create = True

  # If a command is not given, default to enter.
  options.enter |= not any(getattr(options, x.dest)
                           for x in commands.option_list)
  options.enter |= bool(chroot_command)

  if options.enter and options.delete and not options.create:
    parser.error("Trying to enter the chroot when --delete "
                 "was specified makes no sense.")

  # Finally, discern if we need to create the chroot.
  chroot_exists = os.path.exists(options.chroot)
  if options.create or options.enter:
    # Only create if it's being wiped, or if it doesn't exist.
    if not options.delete and chroot_exists:
      options.create = False
    else:
      options.download = True

  # Finally, flip create if necessary.
  if options.enter:
    options.create |= not chroot_exists

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

  lock_path = os.path.dirname(options.chroot)
  lock_path = os.path.join(lock_path,
                           '.%s_lock' % os.path.basename(options.chroot))
  with cgroups.SimpleContainChildren('cros_sdk'):
    with locking.FileLock(lock_path, 'chroot lock') as lock:

      if options.delete and os.path.exists(options.chroot):
        lock.write_lock()
        DeleteChroot(options.chroot)

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
          os.rename(src, target)
        else:
          # Upgrade occurred once already, but either a reversion or
          # some before/after separate cros_sdk usage is at play.
          # Wipe and continue.
          osutils.RmDir(src)

      if options.download:
        lock.write_lock()
        sdk_tarball = FetchRemoteTarballs(sdk_cache, urls)

      if options.create:
        lock.write_lock()
        CreateChroot(options.chroot, sdk_tarball, options.cache_dir,
                     nousepkg=(options.bootstrap or options.nousepkg))

      if options.enter:
        lock.read_lock()
        EnterChroot(options.chroot, options.cache_dir, options.chrome_root,
                    options.chrome_root_mount, chroot_command)
