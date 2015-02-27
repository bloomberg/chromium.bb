# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for running gdb.

This handles the fun details like running against the right sysroot, via
qemu, bind mounts, etc...
"""

from __future__ import print_function

import argparse
import contextlib
import errno
import os
import sys
import tempfile

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import namespaces
from chromite.lib import osutils
from chromite.lib import qemu
from chromite.lib import retry_util

GDB = '/usr/bin/gdb'


class BoardSpecificGdb(object):
  """Framework for running gdb."""

  _BIND_MOUNT_PATHS = ('dev', 'dev/pts', 'proc', 'mnt/host/source', 'sys')

  def __init__(self, board, gdb_args, inf_cmd, inf_args):
    self.board = board
    self.sysroot = cros_build_lib.GetSysroot(board=self.board)
    self.prompt = '(%s-gdb) ' % self.board
    self.host = False
    self.run_as_root = False # May add an option to change this later.
    self.gdb_args = gdb_args
    self.inf_cmd = inf_cmd
    self.inf_args = inf_args
    self.framework = 'auto'
    self.qemu = None

    qemu_arch = qemu.Qemu.DetectArch(GDB, self.sysroot)
    if qemu_arch is None:
      self.framework = 'ldso'
    else:
      self.framework = 'qemu'
      self.qemu = qemu.Qemu(self.sysroot, arch=qemu_arch)

    if not os.path.isdir(self.sysroot):
      raise AssertionError('Sysroot does not exist: %s' % self.sysroot)

  def removeSysrootPrefix(self, path):
    """Returns the given path with any sysroot prefix removed."""
    # If the sysroot is /, then the paths are already normalized.
    if self.sysroot != '/' and path.startswith(self.sysroot):
      path = path.replace(self.sysroot, '', 1)

    return path

  @staticmethod
  def GetNonRootAccount():
    """Return details about the non-root account we want to use.

    Returns:
      A tuple of (username, uid, gid, home).
    """
    return (
        os.environ.get('SUDO_USER', 'nobody'),
        int(os.environ.get('SUDO_UID', '65534')),
        int(os.environ.get('SUDO_GID', '65534')),
        # Should we find a better home?
        '/tmp/portage',
    )

  @staticmethod
  @contextlib.contextmanager
  def LockDb(db):
    """Lock an account database.

    We use the same algorithm as shadow/user.eclass.  This way we don't race
    and corrupt things in parallel.
    """
    lock = '%s.lock' % db
    _, tmplock = tempfile.mkstemp(prefix='%s.platform.' % lock)

    # First try forever to grab the lock.
    retry = lambda e: e.errno == errno.EEXIST
    # Retry quickly at first, but slow down over time.
    try:
      retry_util.GenericRetry(retry, 60, os.link, tmplock, lock, sleep=0.1)
    except Exception:
      print('error: could not grab lock %s' % lock)
      raise

    # Yield while holding the lock, but try to clean it no matter what.
    try:
      os.unlink(tmplock)
      yield lock
    finally:
      os.unlink(lock)

  def SetupUser(self):
    """Propogate the user name<->id mapping from outside the chroot.

    Some unittests use getpwnam($USER), as does bash.  If the account
    is not registered in the sysroot, they get back errors.
    """
    MAGIC_GECOS = 'Added by your friendly platform test helper; do not modify'
    # This is kept in sync with what sdk_lib/make_chroot.sh generates.
    SDK_GECOS = 'ChromeOS Developer'

    user, uid, gid, home = self.GetNonRootAccount()
    if user == 'nobody':
      return

    passwd_db = os.path.join(self.sysroot, 'etc', 'passwd')
    with self.LockDb(passwd_db):
      data = osutils.ReadFile(passwd_db)
      accts = data.splitlines()
      for acct in accts:
        passwd = acct.split(':')
        if passwd[0] == user:
          # Did the sdk make this account?
          if passwd[4] == SDK_GECOS:
            # Don't modify it (see below) since we didn't create it.
            return

          # Did we make this account?
          if passwd[4] != MAGIC_GECOS:
            raise RuntimeError('your passwd db (%s) has unmanaged acct %s' %
                               (passwd_db, user))

          # Maybe we should see if it needs to be updated?  Like if they
          # changed UIDs?  But we don't really check that elsewhere ...
          return

      acct = '%(name)s:x:%(uid)s:%(gid)s:%(gecos)s:%(homedir)s:%(shell)s' % {
          'name': user,
          'uid': uid,
          'gid': gid,
          'gecos': MAGIC_GECOS,
          'homedir': home,
          'shell': '/bin/bash',
      }
      with open(passwd_db, 'a') as f:
        if data[-1] != '\n':
          f.write('\n')
        f.write('%s\n' % acct)

  def run(self):
    """Runs the debugger in a proper environment (e.g. qemu)."""
    self.SetupUser()

    if self.framework == 'qemu':
      self.qemu.Install(self.sysroot)
      self.qemu.RegisterBinfmt()

    for mount in self._BIND_MOUNT_PATHS:
      path = os.path.join(self.sysroot, mount)
      osutils.SafeMakedirs(path)
      osutils.Mount('/' + mount, path, 'none', osutils.MS_BIND)

    gdb_cmd = GDB
    inferior_cmd = self.removeSysrootPrefix(self.inf_cmd)
    gdb_argv = self.gdb_args[:]
    if gdb_argv:
      gdb_argv[0] = self.removeSysrootPrefix(gdb_argv[0])

    # Some programs expect to find data files via $CWD, so doing a chroot
    # and dropping them into / would make them fail.
    cwd = self.removeSysrootPrefix(os.getcwd())

    print('chroot: %s' % self.sysroot)
    print('cwd: %s' % cwd)
    if gdb_argv:
      print('cmd: {%s} %s' % (gdb_cmd, ' '.join(map(repr, gdb_argv))))
    os.chroot(self.sysroot)
    os.chdir(cwd)
    # The TERM the user is leveraging might not exist in the sysroot.
    # Force a sane default that supports standard color sequences.
    os.environ['TERM'] = 'ansi'
    # Some progs want this like bash else they get super confused.
    os.environ['PWD'] = cwd
    if not self.run_as_root:
      _, uid, gid, home = self.GetNonRootAccount()
      os.setgid(gid)
      os.setuid(uid)
      os.environ['HOME'] = home

    gdb_commands = [
        'set sysroot /',
        'set solib-absolute-prefix /',
        'set solib-search-path /',
        'set debug-file-directory /usr/lib/debug',
        'set prompt %s' % self.prompt
    ]

    if self.inf_args:
      arg_str = self.inf_args[0]
      for arg in self.inf_args[1:]:
        arg_str += ' %s' % arg
      gdb_commands.append('set args %s' % arg_str)

    print ("gdb_commands: %s" % repr(gdb_commands))

    gdb_args = [gdb_cmd] + ['--eval-command=%s' % x for x in gdb_commands]
    gdb_args += self.gdb_args

    if inferior_cmd:
      gdb_args.append(inferior_cmd)

    print ("args: %s" % repr(gdb_args))
    sys.exit(os.execvp(gdb_cmd, gdb_args))


def _ReExecuteIfNeeded(argv, ns_net=False, ns_pid=False):
  """Re-execute gdb as root.

  We often need to do things as root, so make sure we're that.  Like chroot
  for proper library environment or do bind mounts.

  Also unshare the mount namespace so as to ensure that doing bind mounts for
  tests don't leak out to the normal chroot.  Also unshare the UTS namespace
  so changes to `hostname` do not impact the host.
  """
  if os.geteuid() != 0:
    cmd = ['sudo', '-E', '--'] + argv
    os.execvp(cmd[0], cmd)
  else:
    namespaces.SimpleUnshare(net=ns_net, pid=ns_pid)


def find_inferior(arg_list):
  """Look for the name of the inferior (to be debugged) in arg list."""

  program_name = ''
  new_list = []
  for item in arg_list:
    if item[0] == '-':
      new_list.append(item)
    elif not program_name:
      program_name = item
    else:
      raise RuntimeError('Found multiple program names: %s  %s'
                         % (program_name, item))

  return program_name, new_list


def main(argv):

  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('--board', required=True,
                      help='board to debug for')
  parser.add_argument('--set_args', dest='set_args', default='',
                      help='Arguments for gdb to pass through to the executable'
                      ' file.')
  parser.add_argument('gdb_args', nargs=argparse.REMAINDER,
                      help='Arguments to gdb itself.  Must come at end of'
                      ' command line.')

  options = parser.parse_args(argv)
  options.Freeze()

  gdb_args = []
  inf_args = []
  inf_cmd = ''

  if options.gdb_args:
    inf_cmd, gdb_args = find_inferior(options.gdb_args)

  if options.set_args:
    inf_args = options.set_args.split()

  if inf_cmd:
    fname = os.path.join(cros_build_lib.GetSysroot(options.board),
                         inf_cmd.lstrip('/'))
    if not os.path.exists(fname):
      cros_build_lib.Die('Cannot find program %s.' % fname)

  if inf_args and not inf_cmd:
    cros_build_lib.Die('Cannot specify arguments without a program.')

  # Once we've finished sanity checking args, make sure we're root.
  _ReExecuteIfNeeded([sys.argv[0]] + argv)

  gdb = BoardSpecificGdb(options.board, gdb_args, inf_cmd, inf_args)

  gdb.run()
