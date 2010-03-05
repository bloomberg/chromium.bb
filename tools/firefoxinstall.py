#!/usr/bin/python
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# Note: If you change this script, make sure the wiki page is up to date:
# http://code.google.com/p/nativeclient/wiki/FilesInstalled


"""Firefox installer for the NaCl developer release

A simple script for a NaCl developer to install the Firefox plugin
on a machine after a NaCl build from source. To be executed from the
root of the NaCl source tree.

"""

import glob
import os
import shutil
import stat
import struct
import sys

def Usage(progname):
  print("""
USAGE:
  %s PLATFORM_BASE=<platform_base>

You don't usually invoke this script directly.
Instead, you use SCons, with the following syntax:
  ./scons [--prebuilt] firefox_install [DBG=1]
where:
 --prebuilt means don't attempt to rebuild the plugin binaries
 DBG=1 means install the debug version of the plugin
For example:
  %s PLATFORM_BASE=.../native_client/scons-out/
is the same as
  ./scons  --prebuilt firefox_install

Note: by default the scons command will install the release version
of the plugin. In order to install the debug version, use:
  ./scons --prebuilt firefox_install DBG=1
which is the same as
  %s PLATFORM_BASE=.../native_client/scons-out/ MODE=1

In this case scons will take care of the command line args.
""" % (progname, progname, progname))


def FatalError(msg):
  sys.stderr.write(msg)
  sys.stderr.write("\n**** INSTALLATION FAILED.\n\n")
  sys.exit(-1)


def CheckCondition(boolcond, emsg):
  if boolcond: return
  FatalError("Error:" + emsg + "\n")


def CheckFileExists(what, emsg):
  if os.path.isfile(what): return
  FatalError("Error: \"%s\" not found.\n%s\n" % (what, emsg))


def CheckDirExists(what, emsg):
  if os.path.isdir(what): return
  FatalError("Error: directory \"%s\" not found.\n%s\n" % (what, emsg))


def OkayToInstall(args):
  if args.get('PKGINSTALL', None):
    return True

  if args.get('SILENT', None):
    return True

  sys.stdout.write("Okay to continue? [y/N] ")
  answer = sys.stdin.readline()
  if answer[0] == "y":
    print "Okay, you asked for it."
    return True
  else:
    print "Okay, I'm bailing out then."
    return False


def CopyFile(src, dst):
  try:
    print "copying", src, "to", dst, "..."
    if os.path.isfile(dst): os.unlink(dst)
    shutil.copy2(src, dst)
  except OSError, err:
    FatalError("ERROR: Could not copy %s to %s: %s\n" % (src, dst, err))


def RenameFile(src, dst):
  try:
    print "rename", src, "to", dst, "..."
    shutil.move(src, dst)
  except OSError, err:
    FatalError("ERROR: Could not rename %s to %s: %s\n" % (src, dst, err))


def CopyDir(src, dst):
  try:
    print "copying directory", src, "to", dst, "..."
    shutil.rmtree(dst, True)
    shutil.copytree(src, dst)
  except OSError, err:
    FatalError("ERROR: Could not copy %s to %s: %s\n" % (src, dst, err))


def CopyFilesOrDirs(src_dir, dst_dir, list):
  for f in list:
    name = os.path.join(src_dir, f)
    if os.path.isfile(name):
      CopyFile(name, dst_dir)
    if os.path.isdir(name):
      target_dir = os.path.join(dst_dir, f)
      CopyDir(name, target_dir)


def RemoveDir(dirname):
  try:
    print "[Removing", dirname, "...]"
    shutil.rmtree(dirname)
  except OSError, err:
    FatalError("ERROR: Could not remove %s: %s\n" % (dirname, err))


def TryMakedir(dirname):
  if not os.path.isdir(dirname):
    try:
      print "[Creating", dirname, "...]"
      os.makedirs(dirname)
    except OSError, err:
      FatalError("ERROR: Could not create %s: %s\n" % (dirname, err))


def RemoveFile(fname):
  try:
    print "[Removing", fname, "...]"
    os.unlink(fname)
  except OSError, err:
    FatalError("ERROR: Could not remove %s: %s\n" % (fname, err))


def RemoveFilesOrDirs(dirname, list):
  for f in list:
    name = os.path.join(dirname, f)
    if os.path.isfile(name):
      RemoveFile(name)
    if os.path.isdir(name):
      RemoveDir(name)


def PrintStars():
  print "*" * 50


def GetTmpDirectory():
  pl = sys.platform
  if pl in ['linux', 'linux2', 'darwin', 'mac']:
    tmp_dir = '/tmp/nacl'
  elif pl in ['win32', 'cygwin', 'win', 'windows']:
    os_tmp = os.getenv('TMP')
    tmp_dir = os.path.join(os_tmp, 'nacl')
  else:
    FatalError('Backup/Restore cannot continue. Unknown platform %s' % pl)
  return tmp_dir


def GetHomeDir():
  # WARNING: may not work on windows
  return os.path.expanduser('~')


def GetPluginDir(args):
  pl = sys.platform
  if pl in ['linux', 'linux2']:
    return os.path.join(GetHomeDir(),'.mozilla/plugins')
  if pl in ['darwin', 'mac']:
    return os.path.join(GetHomeDir(), 'Library', 'Internet Plug-Ins')
  elif pl in ['win32', 'cygwin', 'win', 'windows']:
    return os.path.join(args['PROGRAMFILES'], 'Mozilla Firefox', 'plugins')
  else:
    FatalError('Unknown platform %s' % pl)


# TODO: needs more work for max
def GetFilesToCopy(dirname):
  """This is needed for backup and restore"""
  pool = []
  pool += glob.glob(os.path.join(dirname, 'sel_ldr*'))
  pool += glob.glob(os.path.join(dirname, '*npGoogleNaClPlugin.*'))
  return [os.path.basename(f) for f in pool]


def FindFileByName(files, name):
  for f in files:
    if f.endswith(name):
      return f
  FatalError('ERROR: no file named %s specified on commandline' % name)
  return None

def InstallSuccess(sel_ldr):
  PrintStars()
  print '* You have successfully installed the NaCl Firefox plugin.'
  print '* As a self-test, please confirm you can run'
  print '*    ', sel_ldr
  print '* from a shell/command prompt. With no args you should see'
  print '*     No nacl file specified'
  print '* on Linux or Mac and no output on Windows.'
  PrintStars()


def RunningOn64BitSystem():
  if (8 == len(struct.pack('P',0))):
    return True
  return False


def Linux_install(files, args, use_sandbox):
  plugin_dir = GetPluginDir(args)
  TryMakedir(plugin_dir)
  for f in files:
    CopyFile(f, plugin_dir)

  # bash hack to sneak in sandbox version of sel_ldr
  if use_sandbox:
    RenameFile(os.path.join(plugin_dir, 'sel_ldr'),
               os.path.join(plugin_dir, 'sel_ldr_bin'))

    RenameFile(os.path.join(plugin_dir, 'sel_ldr.trace'),
               os.path.join(plugin_dir, 'sel_ldr_bin.trace'))

    RenameFile(os.path.join(plugin_dir, 'sel_ldr.bash'),
               os.path.join(plugin_dir, 'sel_ldr'))

    os.chmod(os.path.join(plugin_dir, 'sel_ldr'),
             stat.S_IRUSR | stat.S_IXUSR | stat.S_IWUSR)

    CheckFileExists('/bin/bash',
                    'No /bin/bash? NaCl needs this to launch sel_ldr')

  if RunningOn64BitSystem():
    # Call the wrapper on 64-bit Linux
    print '64bit system detected running nspluginwrapper'
    cmd = 'nspluginwrapper -i %s' % os.path.join(plugin_dir,
                                                 'libnpGoogleNaClPlugin.so')
    print cmd
    os.system(cmd)
  InstallSuccess(os.path.join(plugin_dir, 'sel_ldr'))


def Windows_install(files, args):
  plugin_dir = GetPluginDir(args)
  TryMakedir(plugin_dir)
  for f in files:
    CopyFile(f, plugin_dir)
  InstallSuccess(os.path.join(plugin_dir, 'sel_ldr.exe'))


def Mac_install_sdl(files, unused_args):
  global_sdl_dir = os.path.join('/Library/Frameworks/SDL.framework')
  local_sdl_dir = os.path.join(GetHomeDir(),'Library/Frameworks/SDL.framework')
  for sdl_dir in [global_sdl_dir, local_sdl_dir]:
    if os.path.isdir(sdl_dir):
       print '*'
       print '* It looks like SDL is already installed as'
       print '*   ', sdl_dir
       print '* For Native Client we recommend SDL Version 1.2.13'
       print '* Please make sure your SDL version is compatible'
       print '* with Native Client.'
       print '*'
       return

  sdl = FindFileByName(files, 'SDL.framework')
  CopyDir(sdl, local_sdl_dir)


def Mac_install(files, args):
  plugin_dir = GetPluginDir(args)
  TryMakedir(plugin_dir)

  bundle_dir = os.path.join(plugin_dir, 'npGoogleNaClPlugin.bundle')
  if os.path.isdir(bundle_dir):
    RemoveDir(bundle_dir)

  bundle = FindFileByName(files, 'npGoogleNaClPlugin.bundle')
  CopyDir(bundle, bundle_dir)

  sel_ldr_dir = os.path.join(bundle_dir, 'Contents', 'Resources')
  sel_ldr = FindFileByName(files, 'sel_ldr')
  CopyFile(sel_ldr, sel_ldr_dir)

  InstallSuccess(os.path.join(sel_ldr_dir, 'sel_ldr'))


def ParseArgv(argv):
  args = {}
  rest = []
  for arg in argv[1:]:
    if (arg.find('=') < 0):
      rest.append(arg)
    else:
      name, value = arg.split('=')
      print('%s=%s' % (name, value))
      args[name] = value
  return args, rest


def Backup(args):
  plugin_dir = GetPluginDir(args)
  tmp_dir = GetTmpDirectory()
  files_to_copy = GetFilesToCopy(plugin_dir)

  if os.path.isdir(tmp_dir):
    RemoveFilesOrDirs(tmp_dir, files_to_copy)
  else:
    # If for some reason there is a file with the same name.
    if os.path.isfile(tmp_dir):
      RemoveFile(tmp_dir)
    TryMakedir(tmp_dir)

  if os.path.exists(plugin_dir):
    CopyFilesOrDirs(plugin_dir, tmp_dir, files_to_copy)


def Restore(args):
  plugin_dir = GetPluginDir(args)

  tmp_dir = GetTmpDirectory()
  files_to_copy = GetFilesToCopy(plugin_dir)

  if os.path.exists(plugin_dir):
    RemoveFilesOrDirs(plugin_dir, files_to_copy)

  if os.path.exists(tmp_dir):
    CopyFilesOrDirs(tmp_dir, plugin_dir, files_to_copy)
    RemoveFilesOrDirs(tmp_dir, files_to_copy)


def main(argv):
  args, files = ParseArgv(argv)
  if args is None:
    Usage(argv[0])
    return -1

  if args['MODE'] == 'BACKUP':
    Backup(args)
    return 0

  if args['MODE'] == 'RESTORE':
    Restore(args)
    return 0

  if args['MODE'] != 'INSTALL':
    FatalError('Error: MODE must be one of BACKUP, RESTORE, INSTALL')
    return -1

  use_sandbox = int(args.get('USE_SANDBOX', '0'))

  # Tell the user what we're going to do
  print 'This script will install the following files:'
  for f in files:
    print f

  if not OkayToInstall(args):
    return -1

  pl = sys.platform
  if pl in ['linux', 'linux2']:
    Linux_install(files, args, use_sandbox)
  elif pl in ['win32', 'cygwin', 'win', 'windows']:
    Windows_install(files, args)
  elif pl in ['darwin', 'mac']:
    if os.uname()[-1] != 'i386':
      FatalError('Cannot install on PPC Macs.')
    if args.get('PKGINSTALL'):
      FatalError('NOT YET IMPLEMENTED')
      return -1
    else:
      Mac_install_sdl(files, args)
      Mac_install(files, args)
  else:
    print 'Install cannot continue. Unknown platform %s' % pl
    Usage(argv[0])
    return -1

  print '* To test this installation please follow the instructions at'
  print '* documentation/getting_started.html#plugin.'
  # TODO: make this work and point at the right page
  # plugin_demo_page = GetFromStaging(platform_base, 'index.html')
  # print '* A test link can be found at: ', plugin_demo_page
  PrintStars()
  return 0


if '__main__' ==  __name__:
  sys.exit(main(sys.argv))
