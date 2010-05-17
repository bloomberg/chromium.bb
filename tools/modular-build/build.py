#!/usr/bin/python

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import glob
import optparse
import os
import sys

import dirtree
import btarget


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


def GetSources():
  gcc_patches = sorted(glob.glob(os.path.join(
              nacl_dir, "tools/patches/*-gcc-4.4.3.patch")))
  assert len(gcc_patches) > 0
  return {
    "binutils": dirtree.PatchedTree(
        dirtree.TarballTree(FindFile("binutils-2.20.tar.bz2")),
        [FindFile("binutils-2.20.patch")]),
    "gcc": dirtree.PatchedTree(
        dirtree.MultiTarballTree(
            [FindFile("gcc-core-4.4.3.tar.bz2"),
             FindFile("gcc-g++-4.4.3.tar.bz2"),
             FindFile("gcc-testsuite-4.4.3.tar.bz2")]),
        gcc_patches),
    "newlib": dirtree.PatchedTree(
        dirtree.TarballTree(FindFile("newlib-1.18.0.tar.gz")),
        [FindFile("newlib-1.18.0.patch")]),
    }


common_gcc_options = [
    "--disable-libmudflap",
    "--disable-decimal-float",
    "--disable-libssp",
    "--disable-libstdcxx-pch",
    "--disable-shared",
    "--target=nacl64"]


def GetTargets(src):
  top_dir = os.path.abspath("out")
  src = dict((src_name,
              btarget.SourceTarget("%s-src" % src_name,
                                  os.path.join(top_dir, "source", src_name),
                                  src_tree))
             for src_name, src_tree in src.iteritems())
  modules = {}
  module_list = []

  def MakeInstallPrefix(name, deps):
    return btarget.UnionDir("%s-input" % name,
                            os.path.join(top_dir, "input-prefix", name),
                            [modules[dep] for dep in deps])

  def AddModule(name, module):
    modules[name] = module
    module_list.append(module)

  def AddAutoconfModule(name, src_name, deps, **kwargs):
    module = btarget.AutoconfModule(
        name,
        os.path.join(top_dir, "install", name),
        os.path.join(top_dir, "build", name),
        MakeInstallPrefix(name, deps), src[src_name], **kwargs)
    AddModule(name, module)

  def AddSconsModule(name, deps, scons_args):
    module = btarget.SconsBuild(
        name,
        os.path.join(top_dir, "install", name),
        modules["nacl_src"],
        MakeInstallPrefix(name, deps), scons_args)
    AddModule(name, module)

  modules["nacl_src"] = btarget.ExistingSource("nacl-src", nacl_dir)
  modules["nacl-headers"] = \
      btarget.ExportHeaders("nacl-headers", os.path.join(top_dir, "headers"),
                            modules["nacl_src"])
  # newlib requires the NaCl headers to be copied into its source directory.
  # TODO(mseaborn): Fix newlib to not require this.
  src["newlib2"] = \
      btarget.UnionDir2("newlib2", os.path.join(top_dir, "newlib2"),
                        [("", src["newlib"]),
                         ("newlib/libc/sys/nacl", modules["nacl-headers"])])
  AddAutoconfModule(
      "binutils", "binutils", deps=[],
      configure_opts=[
          "--target=nacl64",
          "CFLAGS=-DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5"])
  # Undeclared Debian dependencies:
  # sudo apt-get install libgmp3-dev libmpfr-dev
  AddAutoconfModule(
      "pre-gcc", "gcc", deps=["binutils"],
      configure_opts=common_gcc_options + [
          "--without-headers",
          "--enable-languages=c",
          "--disable-threads"],
      # CFLAGS has to be passed via environment because the
      # configure script can't cope with spaces otherwise.
      configure_env=[
          "CC=gcc",
          "CFLAGS=-Dinhibit_libc -D__gthr_posix_h "
          "-DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5"],
      # The default make target doesn't work - it gives libiberty
      # configure failures.  Need to do "all-gcc" instead.
      make_cmd=["make", "all-gcc"],
      install_cmd=["make", "install-gcc"])
  AddAutoconfModule(
      "newlib", "newlib2", deps=["binutils", "pre-gcc"],
      configure_opts=[
          "--disable-libgloss",
          "--enable-newlib-io-long-long",
          "--enable-newlib-io-c99-formats",
          "--enable-newlib-mb",
          "--target=nacl64"],
      configure_env=["CFLAGS=-O2"],
      make_cmd=["make", "CFLAGS_FOR_TARGET=-m64 -O2"])

  AddSconsModule(
      "nc_threads",
      deps=["binutils", "pre-gcc"],
      scons_args=["MODE=nacl_extra_sdk", "install_libpthread"])
  AddSconsModule(
      "libnacl_headers",
      deps=["binutils", "pre-gcc"],
      scons_args=["MODE=nacl_extra_sdk", "extra_sdk_update_header"])
  AddSconsModule(
      "libnacl",
      deps=["binutils", "pre-gcc", "newlib", "libnacl_headers", "nc_threads"],
      scons_args=["MODE=nacl_extra_sdk", "extra_sdk_update", "nocpp=yes"])

  AddAutoconfModule(
      "full-gcc", "gcc",
      deps=["binutils", "newlib", "libnacl", "libnacl_headers", "nc_threads"],
      configure_opts=common_gcc_options + [
          "--with-newlib",
          "--enable-threads=nacl",
          "--enable-tls",
          "--disable-libgomp",
          "--enable-languages=c,c++"],
      configure_env=[
          "CC=gcc",
          "CFLAGS=-Dinhibit_libc -DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5"],
      make_cmd=["make", "all"])

  hello_c = """
#include <stdio.h>
int main() {
  printf("Hello world\\n");
  return 0;
}
"""
  modules["hello"] = btarget.TestModule(
      "hello",
      os.path.join(top_dir, "build", "hello"),
      MakeInstallPrefix("hello", ["newlib", "full-gcc", "libnacl", "binutils"]),
      hello_c)
  module_list.append(modules["hello"])
  return module_list


def Main(args):
  # TODO(mseaborn): Add the ability to build subsets of targets.
  parser = optparse.OptionParser()
  parser.add_option("-b", "--build", dest="do_build", action="store_true",
                    help="Do the build")
  options, args = parser.parse_args(args)
  mods = GetTargets(GetSources())
  btarget.PrintPlan(mods, sys.stdout)
  if options.do_build:
    btarget.Rebuild(mods)


if __name__ == "__main__":
  Main(sys.argv[1:])
