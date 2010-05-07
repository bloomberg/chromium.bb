#!/usr/bin/python

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import glob
import os
import shutil
import subprocess
import sys

import action_tree
import cmd_env


script_dir = os.path.abspath(os.path.dirname(__file__))
# This allows "src" to be a symlink pointing to NaCl's "trunk/src".
nacl_src = os.path.join(script_dir, "src")
# Otherwise we expect to live inside the NaCl tree.
if not os.path.exists(nacl_src):
  nacl_src = os.path.normpath(os.path.join(script_dir, "..", "..", ".."))
nacl_dir = os.path.join(nacl_src, "native_client")

subdirs = [
    "third_party/gcc",
    "third_party/binutils",
    "third_party/newlib",
    "native_client/tools/patches"]
search_path = [os.path.join(nacl_src, subdir) for subdir in subdirs]


def FindFile(name):
  for dir_path in search_path:
    filename = os.path.join(dir_path, name)
    if os.path.exists(filename):
      return filename
  raise Exception("Couldn't find %r in %r" % (name, search_path))


def GetOne(lst):
  assert len(lst) == 1, lst
  return lst[0]


def WriteFile(filename, data):
  fh = open(filename, "w")
  try:
    fh.write(data)
  finally:
    fh.close()


def MkdirP(dir_path):
  subprocess.check_call(["mkdir", "-p", dir_path])


class DirTree(object):

  # WriteTree(dest_dir) makes a fresh copy of the tree in dest_dir.
  # It can assume that dest_dir is initially empty.
  # The state of dest_dir is undefined if WriteTree() fails.
  def WriteTree(self, env, dest_dir):
    raise NotImplementedError()


class EmptyTree(DirTree):

  def WriteTree(self, env, dest_dir):
    pass


class TarballTree(DirTree):

  def __init__(self, tar_path):
    self._tar_path = tar_path

  def WriteTree(self, env, dest_dir):
    # Tarballs normally contain a single top-level directory with
    # a name like foo-module-1.2.3.  We strip this off.
    assert os.listdir(dest_dir) == []
    env.cmd(["tar", "-C", dest_dir, "-xf", self._tar_path])
    tar_name = GetOne(os.listdir(dest_dir))
    for leafname in os.listdir(os.path.join(dest_dir, tar_name)):
      os.rename(os.path.join(dest_dir, tar_name, leafname),
                os.path.join(dest_dir, leafname))
    os.rmdir(os.path.join(dest_dir, tar_name))


# This handles gcc, where two source tarballs must be unpacked on top
# of each other.
class MultiTarballTree(DirTree):

  def __init__(self, tar_paths):
    self._tar_paths = tar_paths

  def WriteTree(self, env, dest_dir):
    assert os.listdir(dest_dir) == []
    for tar_file in self._tar_paths:
      env.cmd(["tar", "-C", dest_dir, "-xf", tar_file])
    tar_name = GetOne(os.listdir(dest_dir))
    for leafname in os.listdir(os.path.join(dest_dir, tar_name)):
      os.rename(os.path.join(dest_dir, tar_name, leafname),
                os.path.join(dest_dir, leafname))
    os.rmdir(os.path.join(dest_dir, tar_name))


class PatchedTree(DirTree):

  def __init__(self, orig_tree, patch_files):
    self._orig_tree = orig_tree
    self._patch_files = patch_files

  def WriteTree(self, env, dest_dir):
    self._orig_tree.WriteTree(env, dest_dir)
    for patch_file in self._patch_files:
      env.cmd(["patch", "-d", dest_dir, "-p1", "-i", patch_file])


class EnvVarEnv(object):

  def __init__(self, envvars, env):
    self._envvars = envvars
    self._env = env

  def cmd(self, args, **kwargs):
    return self._env.cmd(
        ["env"] + ["%s=%s" % (key, value) for key, value in self._envvars]
        + args, **kwargs)


class ModuleBase(object):

  def __init__(self, source_dir, build_dir, prefix, install_dir, env_vars):
    self._env = cmd_env.VerboseWrapper(cmd_env.BasicEnv())
    self._source_dir = source_dir
    self._build_dir = build_dir
    self._prefix = prefix
    self._install_dir = install_dir
    self._build_env = cmd_env.PrefixCmdEnv(
        cmd_env.in_dir(self._build_dir), EnvVarEnv(env_vars, self._env))
    self._args = {"prefix": self._prefix,
                  "source_dir": self._source_dir}

  def all(self):
    return action_tree.make_node(
        [self.unpack, self.configure, self.make, self.install], self.name)

  def unpack(self, log):
    if not os.path.exists(self._source_dir):
      temp_dir = "%s.temp" % self._source_dir
      os.makedirs(temp_dir)
      self.source.WriteTree(self._env, temp_dir)
      os.rename(temp_dir, self._source_dir)


def RemoveTree(dir_path):
  if os.path.exists(dir_path):
    shutil.rmtree(dir_path)


def CopyOnto(source_dir, dest_dir):
  for leafname in os.listdir(source_dir):
    subprocess.check_call(["cp", "-a", os.path.join(source_dir, leafname),
                           "-t", dest_dir])


def MungeMultilibDir(install_dir):
  # This is done instead of the lib32 -> lib/32 symlink that Makefile uses.
  # TODO(mseaborn): Fix newlib to not output using this odd layout.
  if os.path.exists(os.path.join(install_dir, "nacl64/lib/32")):
    os.rename(os.path.join(install_dir, "nacl64/lib/32"),
              os.path.join(install_dir, "nacl64/lib32"))


def InstallDestdir(prefix_dir, install_dir, func):
  temp_dir = "%s.tmp" % install_dir
  RemoveTree(temp_dir)
  func(temp_dir)
  RemoveTree(install_dir)
  # Tree is installed into $DESTDIR/$prefix.
  # We need to strip $prefix.
  assert prefix_dir.startswith("/")
  temp_subdir = os.path.join(temp_dir, prefix_dir.lstrip("/"))
  MungeMultilibDir(temp_subdir)
  os.rename(temp_subdir, install_dir)
  # TODO: assert that temp_dir doesn't contain anything except prefix dirs
  RemoveTree(temp_dir)
  MkdirP(prefix_dir)
  CopyOnto(install_dir, prefix_dir)


binutils_tree = PatchedTree(TarballTree(FindFile("binutils-2.20.tar.bz2")),
                            [FindFile("binutils-2.20.patch")])
gcc_patches = sorted(glob.glob(os.path.join(
      nacl_dir, "tools/patches/*-gcc-4.4.3.patch")))
assert len(gcc_patches) > 0
gcc_tree = PatchedTree(MultiTarballTree(
                           [FindFile("gcc-core-4.4.3.tar.bz2"),
                            FindFile("gcc-g++-4.4.3.tar.bz2"),
                            FindFile("gcc-testsuite-4.4.3.tar.bz2")]),
                       gcc_patches)
newlib_tree = PatchedTree(TarballTree(FindFile("newlib-1.18.0.tar.gz")),
                          [FindFile("newlib-1.18.0.patch")])


def Module(name, source, configure_cmd, make_cmd, install_cmd):
  # TODO: this nested class is ugly
  class Mod(ModuleBase):

    # These assignments don't work because of Python's odd scoping rules:
    # name = name
    # source = source

    def _Subst(self, Cmd):
      return [arg % self._args for arg in Cmd]

    def configure(self, log):
      MkdirP(self._build_dir)
      self._build_env.cmd(self._Subst(configure_cmd))

    def make(self, log):
      self._build_env.cmd(self._Subst(make_cmd))

    def install(self, log):
      def run(dest):
        Cmd = [arg % {"destdir": dest} for arg in install_cmd]
        self._build_env.cmd(Cmd)
      InstallDestdir(self._prefix, self._install_dir, run)

  Mod.name = name
  Mod.source = source
  return Mod


ModuleBinutils = Module(
    name="binutils",
    source=binutils_tree,
    configure_cmd=[
        "sh", "-c",
        "%(source_dir)s/configure "
        'CFLAGS="-DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5" '
        "--prefix=%(prefix)s "
        "--target=nacl64"],
    make_cmd=["make", "-j4"],
    install_cmd=["make", "install", "DESTDIR=%(destdir)s"])


common_gcc_options = (
    "--disable-libmudflap "
    "--disable-decimal-float "
    "--disable-libssp "
    "--disable-libstdcxx-pch "
    "--disable-shared "
    "--prefix=%(prefix)s "
    "--target=nacl64 ")

# Dependencies: libgmp3-dev libmpfr-dev
ModulePregcc = Module(
    name="pregcc",
    source=gcc_tree,
    # CFLAGS has to be passed via environment because the
    # configure script can't cope with spaces otherwise.
    configure_cmd=[
        "sh", "-c",
        "CC=gcc "
        'CFLAGS="-Dinhibit_libc -D__gthr_posix_h '
            '-DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5" '
        "%(source_dir)s/configure "
        "--without-headers "
        "--enable-languages=c "
        "--disable-threads " # pregcc
        + common_gcc_options],
    # The default make target doesn't work - it gives libiberty
    # configure failures.  Need to do "all-gcc" instead.
    make_cmd=["make", "all-gcc", "-j2"],
    install_cmd=["make", "install-gcc", "DESTDIR=%(destdir)s"])

ModuleFullgcc = Module(
    name="fullgcc",
    source=gcc_tree,
    # CFLAGS has to be passed via environment because the
    # configure script can't cope with spaces otherwise.
    configure_cmd=[
        "sh", "-c",
        "CC=gcc "
        'CFLAGS="-Dinhibit_libc -DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5" '
        "%(source_dir)s/configure "
        "--with-newlib "
        "--enable-threads=nacl "
        "--enable-tls "
        "--disable-libgomp "
        '--enable-languages="c,c++" '
        + common_gcc_options],
    make_cmd=["make", "all", "-j2"],
    install_cmd=["make", "install", "DESTDIR=%(destdir)s"])


class ModuleNewlib(ModuleBase):

  name = "newlib"
  source = newlib_tree

  def configure(self, log):
    # This is like exporting the kernel headers to glibc.
    # This should be done differently.
    self._env.cmd(
        [os.path.join(nacl_dir,
                      "src/trusted/service_runtime/export_header.py"),
         os.path.join(nacl_dir, "src/trusted/service_runtime/include"),
         os.path.join(self._source_dir, "newlib/libc/sys/nacl")])

    MkdirP(self._build_dir)
    # CFLAGS has to be passed via environment because the
    # configure script can't cope with spaces otherwise.
    self._build_env.cmd([
            "sh", "-c",
            'CFLAGS="-O2" '
            "%(source_dir)s/configure "
            "--disable-libgloss "
            "--enable-newlib-io-long-long "
            "--enable-newlib-io-c99-formats "
            "--enable-newlib-mb "
            "--prefix=%(prefix)s "
            "--target=nacl64"
            % self._args])

  def make(self, log):
    self._build_env.cmd(["sh", "-c", "make", "CFLAGS_FOR_TARGET=-m64 -O2"])

  def install(self, log):
    InstallDestdir(
        self._prefix, self._install_dir,
        lambda dest: self._build_env.cmd(["make", "install",
                                          "DESTDIR=%s" % dest]))


class ModuleNcthreads(ModuleBase):

  name = "nc_threads"
  source = EmptyTree()

  def configure(self, log):
    pass

  def make(self, log):
    pass

  def install(self, log):
    MkdirP(self._build_dir)
    def DoMake(dest):
      self._build_env.cmd(
          cmd_env.in_dir(nacl_dir) +
          ["./scons", "MODE=nacl_extra_sdk", "install_libpthread",
           "USE_ENVIRON=1",
           "naclsdk_mode=custom:%s" %
           os.path.join(dest, self._prefix.lstrip("/")),
           "naclsdk_validate=0",
           "--verbose"])
    InstallDestdir(self._prefix, self._install_dir, DoMake)


class ModuleLibnaclHeaders(ModuleBase):

  name = "libnacl_headers"
  source = EmptyTree()

  def configure(self, log):
    pass

  def make(self, log):
    pass

  def install(self, log):
    MkdirP(self._build_dir)
    # This requires scons to pass PATH through so that it can run
    # nacl-gcc.  We set naclsdk_mode to point to an empty
    # directory so it can't get nacl-gcc from there.  However, if
    # scons-out is already populated, scons won't try to run
    # nacl-gcc.
    def DoMake(dest):
      self._build_env.cmd(
          cmd_env.in_dir(nacl_dir) +
          ["./scons", "MODE=nacl_extra_sdk", "extra_sdk_update_header",
           "USE_ENVIRON=1",
           "nocpp=yes",
           "naclsdk_mode=custom:%s" %
           os.path.join(dest, self._prefix.lstrip("/")),
           "naclsdk_validate=0",
           "--verbose"])
    InstallDestdir(self._prefix, self._install_dir, DoMake)


class ModuleLibnacl(ModuleBase):

  # Covers libnacl.a, crt[1ni].o and misc libraries built with Scons.
  name = "libnacl"
  source = EmptyTree()

  def configure(self, log):
    pass

  def make(self, log):
    pass

  def install(self, log):
    MkdirP(self._build_dir)
    # This requires scons to pass PATH through so that it can run
    # nacl-gcc.  We set naclsdk_mode to point to an empty
    # directory so it can't get nacl-gcc from there.  However, if
    # scons-out is already populated, scons won't try to run
    # nacl-gcc.
    def DoMake(dest):
      self._build_env.cmd(
          cmd_env.in_dir(nacl_dir) +
          ["./scons", "MODE=nacl_extra_sdk", "extra_sdk_update",
           "USE_ENVIRON=1",
           "nocpp=yes",
           "naclsdk_mode=custom:%s" %
           os.path.join(dest, self._prefix.lstrip("/")),
           "naclsdk_validate=0",
           "--verbose"])
    InstallDestdir(self._prefix, self._install_dir, DoMake)


class TestModule(ModuleBase):

  name = "test"
  source = EmptyTree()

  def configure(self, log):
    pass

  def make(self, log):
    MkdirP(self._build_dir)
    WriteFile(os.path.join(self._build_dir, "hellow.c"), """
#include <stdio.h>
int main() {
  printf("Hello world\\n");
  return 0;
}
""")
    self._build_env.cmd(["sh", "-c", "nacl64-gcc -m32 hellow.c -o hellow"])

  def install(self, log):
    pass


def AddToPath(path, dir_path):
  return "%s:%s" % (dir_path, path)


mods = [
    ModuleBinutils,
    ModulePregcc,
    ModuleNewlib,
    ModuleNcthreads,
    ModuleFullgcc,
    ModuleLibnaclHeaders,
    ModuleLibnacl,
    TestModule,
    ]


def AllMods(top_dir, use_shared_prefix):
  nodes = []
  env_vars = []
  path_dirs = []

  source_base = os.path.join(top_dir, "source")
  if use_shared_prefix:
    base_dir = os.path.join(top_dir, "shared")
    prefix = os.path.join(base_dir, "prefix")
    path_dirs.append(os.path.join(prefix, "bin"))
  else:
    base_dir = os.path.join(top_dir, "split")
    prefix_base = os.path.join(base_dir, "prefixes")

  for mod in mods:
    if not use_shared_prefix:
      # TODO: In split-prefix case, we don't really need "prefix" dirs.
      # Just use the "install" dirs.
      prefix = os.path.join(prefix_base, mod.name)
      path_dirs.append(os.path.join(prefix, "bin"))
    source_dir = os.path.join(source_base, mod.name)
    build_dir = os.path.join(base_dir, "build", mod.name)
    install_dir = os.path.join(base_dir, "install", mod.name)
    builder = mod(source_dir, build_dir, prefix, install_dir, env_vars)
    nodes.append(builder.all())

  env_vars.append(("PATH",
                   reduce(AddToPath, path_dirs, os.environ["PATH"])))
  return action_tree.make_node(nodes, name="all")


def Main(args):
  base_dir = os.getcwd()
  top = AllMods(base_dir, use_shared_prefix=True)
  action_tree.action_main(top, args)


if __name__ == "__main__":
  Main(sys.argv[1:])
