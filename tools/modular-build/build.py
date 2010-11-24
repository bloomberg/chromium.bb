#!/usr/bin/python

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import glob
import os
import sys

import dirtree
import btarget
import treemappers


script_dir = os.path.abspath(os.path.dirname(__file__))
# This allows "src" to be a symlink pointing to NaCl's "trunk/src".
nacl_src = os.path.join(script_dir, "src")
# Otherwise we expect to live inside the NaCl tree.
if not os.path.exists(nacl_src):
  nacl_src = os.path.normpath(os.path.join(script_dir, "..", "..", ".."))
nacl_dir = os.path.join(nacl_src, "native_client")

subdirs = [
    "third_party/gmp",
    "third_party/mpfr",
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


def PatchGlob(name):
  path = os.path.join(nacl_dir, "tools/patches", name, "*.patch")
  patches = sorted(glob.glob(path))
  if len(patches) == 0:
    raise AssertionError("No patches found matching %r" % path)
  return patches


def ParseKeyValueFile(filename):
  fh = open(filename, "r")
  data = {}
  for line in fh:
    line = line.strip()
    if line.startswith("#") or line == "":
      # Ignore comments and empty lines.
      pass
    elif "=" in line:
      key, value = line.split("=", 1)
      if key in data:
        raise Exception("Duplicate key in file: %r" % line)
      data[key] = value
    else:
      raise Exception("Unrecognised line: %r" % line)
  fh.close()
  return data


def GetSources(dest_dir):
  src = {}

  def AddSource(name, tree):
    src[name] = btarget.SourceTarget(
        "%s-src" % name, os.path.join(dest_dir, name), tree)

  def AddGitSource(name, url, commit_id):
    src[name] = btarget.SourceTargetGit(
        "%s-src" % name, os.path.join(dest_dir, name), url, commit_id)

  revisions = ParseKeyValueFile(os.path.join(nacl_dir, "tools/REVISIONS"))
  AddSource("gmp", dirtree.TarballTree(FindFile("gmp-4.3.1.tar.bz2")))
  AddSource("mpfr", dirtree.TarballTree(FindFile("mpfr-2.4.1.tar.bz2")))
  AddSource("binutils",
            dirtree.PatchedTree(
                dirtree.TarballTree(FindFile("binutils-2.20.1.tar.bz2")),
                PatchGlob("binutils-2.20.1"), strip=2))
  AddSource("gcc",
            dirtree.PatchedTree(
                dirtree.MultiTarballTree(
                    [FindFile("gcc-core-4.4.3.tar.bz2"),
                     FindFile("gcc-g++-4.4.3.tar.bz2"),
                     FindFile("gcc-testsuite-4.4.3.tar.bz2")]),
                PatchGlob("gcc-4.4.3"), strip=2))
  AddSource("newlib",
            dirtree.PatchedTree(
                dirtree.TarballTree(FindFile("newlib-1.18.0.tar.gz")),
                PatchGlob("newlib-1.18.0"), strip=2))
  # For a discussion of why nacl-glibc uses these headers, see
  # http://code.google.com/p/nativeclient/issues/detail?id=671
  AddGitSource("linux_headers",
               "http://git.chromium.org/git/linux-headers-for-nacl.git",
               commit_id=revisions["LINUX_HEADERS_FOR_NACL_COMMIT"])
  AddGitSource("glibc",
               "http://git.chromium.org/git/nacl-glibc.git",
               commit_id=revisions["NACL_GLIBC_COMMIT"])
  return src


def GetTargets():
  top_dir = os.path.abspath("out")
  src = GetSources(os.path.join(top_dir, "source"))
  modules = {}
  module_list = []

  # In principle we can build a multilib toolchain with either
  # "--target=nacl" or "--target=nacl64", and it should only affect
  # the default subarch.  In practice, the "--target" option affects
  # install locations and there are various inconsistencies.
  arch = "nacl64"
  subarches = ["32", "64"]
  # The default subarch usually goes into "lib".
  subarch_to_libdir = {"32": "lib32", "64": "lib"}

  def MakeInstallPrefix(name, deps):
    return btarget.TreeMapper("%s-input" % name,
                              os.path.join(top_dir, "input-prefix", name),
                              treemappers.CombineInstallTrees,
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

  def AddSconsModule(name, deps, scons_args, **kwargs):
    module = btarget.SconsBuild(
        name,
        os.path.join(top_dir, "install", name),
        os.path.join(top_dir, "build", name),
        modules["nacl_src"],
        MakeInstallPrefix(name, deps), scons_args, arch, **kwargs)
    AddModule(name, module)

  if sys.platform == "darwin":
    # libgmp's configure script has a special case which builds it
    # with -m64 by default on Mac OS X.  (Maybe this was for PowerPC
    # rather than x86?)  That will not work when everything else uses
    # the host gcc's default of -m32 (except for mpfr, which gets its
    # CFLAGS from gmp.h!).  So we need to override this.
    gmp_opts = ["--build=i386-apple-darwin"]
  else:
    gmp_opts = []
  AddAutoconfModule("gmp", "gmp", configure_opts=gmp_opts, deps=[])
  AddAutoconfModule("mpfr", "mpfr", deps=["gmp"])
  # TODO(mseaborn): Add an automated mechanism for these libraries to
  # be pulled in whenever gcc is declared as a dependency.
  gcc_libs = ["gmp", "mpfr"]

  common_gcc_options = [
      "--disable-libgomp",
      "--disable-libmudflap",
      "--disable-decimal-float",
      "--disable-libssp",
      "--disable-libstdcxx-pch",
      "--disable-shared",
      "--target=%s" % arch]

  modules["nacl_src"] = btarget.ExistingSource("nacl-src", nacl_dir)
  modules["nacl-headers"] = \
      btarget.ExportHeaders("nacl-headers", os.path.join(top_dir, "headers"),
                            modules["nacl_src"])
  # newlib requires the NaCl headers to be copied into its source directory.
  # TODO(mseaborn): Fix newlib to not require this.
  src["newlib2"] = btarget.TreeMapper(
      "newlib2", os.path.join(top_dir, "newlib2"),
      treemappers.AddHeadersToNewlib,
      [src["newlib"], modules["nacl-headers"]])
  AddAutoconfModule(
      "binutils", "binutils", deps=[],
      configure_opts=[
          "--target=%s" % arch,
          "CFLAGS=-DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5"])
  AddAutoconfModule(
      "pre-gcc", "gcc", deps=["binutils"] + gcc_libs,
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
      make_cmd=["make", "all-gcc", "all-target-libgcc"],
      install_cmd=["make", "install-gcc", "install-target-libgcc"])
  AddAutoconfModule(
      "newlib", "newlib2", deps=["binutils", "pre-gcc"] + gcc_libs,
      configure_opts=[
          "--disable-libgloss",
          "--enable-newlib-io-long-long",
          "--enable-newlib-io-c99-formats",
          "--enable-newlib-mb",
          "--target=%s" % arch],
      configure_env=["CFLAGS=-O2"],
      make_cmd=["make", "CFLAGS_FOR_TARGET=-O2"])

  AddSconsModule(
      "nc_threads",
      # This only installs headers, so it has no dependencies.
      deps=[],
      scons_args=["MODE=nacl_extra_sdk", "install_libpthread",
                  "naclsdk_validate=0"])
  AddSconsModule(
      "libnacl_headers",
      deps=[],
      scons_args=["MODE=nacl_extra_sdk", "extra_sdk_update_header",
                  "naclsdk_validate=0"])
  # Before full-gcc is built, we cannot build any C++ code, and
  # tools/Makefile builds the following with nocpp=yes.  However,
  # full-gcc does not actually depend on it, so we do not use it.
  AddSconsModule(
      "libnacl_nocpp",
      deps=["binutils", "pre-gcc", "newlib", "libnacl_headers", "nc_threads"] +
          gcc_libs,
      scons_args=["MODE=nacl_extra_sdk", "extra_sdk_update", "nocpp=yes"])

  AddAutoconfModule(
      "full-gcc", "gcc",
      deps=["binutils", "newlib", "libnacl_headers", "nc_threads"] + gcc_libs,
      configure_opts=common_gcc_options + [
          "--with-newlib",
          "--enable-threads=nacl",
          "--enable-tls",
          "--enable-languages=c,c++"],
      configure_env=[
          "CC=gcc",
          "CFLAGS=-Dinhibit_libc -DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5"],
      make_cmd=["make", "all"])

  for arch_bits in subarches:
    AddSconsModule(
        "libnacl_x86_%s" % arch_bits,
        deps=["binutils", "full-gcc", "newlib",
              "libnacl_headers", "nc_threads"] + gcc_libs,
        scons_args=["MODE=nacl_extra_sdk", "extra_sdk_update",
                    "platform=x86-%s" % arch_bits],
        libdir=subarch_to_libdir[arch_bits])

  # Note that ordering is significant in the dependencies: nc_threads
  # must come after newlib in order to override newlib's pthread.h.
  newlib_toolchain = MakeInstallPrefix(
      "newlib_toolchain",
      deps=["binutils", "full-gcc", "newlib", "nc_threads",
            "libnacl_x86_32", "libnacl_x86_64"] + gcc_libs)

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
      newlib_toolchain,
      hello_c,
      compiler=["%s-gcc" % arch, "-m32"])
  module_list.append(modules["hello"])

  # glibc invokes "readelf" in a configure check without an
  # architecture prefix (such as "nacl-"), which is correct because
  # readelf knows only about ELF and is otherwise architecture
  # neutral.  Create readelf as an alias for nacl-readelf so that
  # glibc can build on Mac OS X, where readelf is not usually
  # installed.
  AddModule("readelf",
            btarget.TreeMapper(
      "readelf", os.path.join(top_dir, "install", "readelf"),
      treemappers.CreateAlias, [], args=["readelf", "%s-readelf" % arch]))

  # nacl-gcc's spec file forces linking against -lcrt_platform and
  # -lnacl, but the former is specific to the newlib toolchain and the
  # latter is not a dependency of glibc's libc.  We work around this
  # by providing dummy libraries.
  # TODO(mseaborn): Change the nacl-gcc spec file to remove "-lnacl"
  # and "-lcrt_platform".
  modules["dummy_libnacl"] = btarget.TreeMapper(
      "dummy_libnacl",
      os.path.join(top_dir, "install", "dummy_libnacl"),
      treemappers.DummyLibrary, [], args=[arch, "libnacl"])
  modules["dummy_libcrt_platform"] = btarget.TreeMapper(
      "dummy_libcrt_platform",
      os.path.join(top_dir, "install", "dummy_libcrt_platform"),
      treemappers.DummyLibrary, [], args=[arch, "libcrt_platform"])
  # We also provide a dummy libnosys for tests that link against it.
  modules["dummy_libnosys"] = btarget.TreeMapper(
      "dummy_libnosys",
      os.path.join(top_dir, "install", "dummy_libnosys"),
      treemappers.DummyLibrary, [], args=[arch, "libnosys"])

  subarch_to_glibc_arch = {"32": "i486-linux-gnu",
                           "64": "x86_64-linux-gnu"}
  subarch_to_glibc_extra_cflags = {"32": " -march=i486",
                                   "64": ""}
  for arch_bits in subarches:
    AddAutoconfModule(
        "glibc_%s" % arch_bits, "glibc",
        deps=["binutils", "pre-gcc", "readelf",
              "dummy_libnacl", "dummy_libcrt_platform"] + gcc_libs,
        explicitly_passed_deps=[src["linux_headers"]],
        configure_opts=[
            "--prefix=/%s" % arch,
            "--libdir=${exec_prefix}/%s" % subarch_to_libdir[arch_bits],
            "--host=%s" % subarch_to_glibc_arch[arch_bits],
            "CC=%s-gcc -m%s" % (arch, arch_bits),
            ("CFLAGS=-pipe -fno-strict-aliasing -O2 "
             "-mno-tls-direct-seg-refs -g"
             + subarch_to_glibc_extra_cflags[arch_bits]),
            ("--with-headers=%s" %
             os.path.join(src["linux_headers"].dest_path, "include")),
            "--enable-kernel=2.2.0"],
        configure_env=[
          # We need these settings because a lack of a crt1.o in the build
          # environment causes the "forced unwind support" autoconf check
          # to fail.  The alternative is to build against full-gcc,
          # libnacl_nocpp and newlib.
          "libc_cv_forced_unwind=yes", "libc_cv_c_cleanup=yes",
          # glibc does not provide a configure option for setting this.
          "libc_cv_slibdir=${exec_prefix}/%s" % subarch_to_libdir[arch_bits]],
        use_install_root=True)

  # TODO(mseaborn): It would be better if installing linker scripts
  # did not require an ad-hoc rule.
  modules["linker_scripts"] = btarget.TreeMapper(
      "linker_scripts",
      os.path.join(top_dir, "install", "linker_scripts"),
      treemappers.InstallLinkerScripts, [src["glibc"]], args=[arch])
  # TODO(mseaborn): One day the NaCl headers should be substitutable
  # for the Linux headers here, but I would expect them to be very
  # similar.  i.e. Same filenames, same #defined numbers, but a subset
  # of the Linux headers.
  modules["installed_linux_headers"] = btarget.TreeMapper(
      "installed_linux_headers",
      os.path.join(top_dir, "install", "linux_headers"),
      treemappers.InstallKernelHeaders, [src["linux_headers"]], args=[arch])
  modules["installed_nacl_headers"] = btarget.TreeMapper(
      "installed_nacl_headers",
      os.path.join(top_dir, "install", "nacl_headers"),
      treemappers.SubsetNaClHeaders, [modules["nacl-headers"]], args=[arch])

  modules["sys_include_alias"] = btarget.TreeMapper(
      "sys_include_alias",
      os.path.join(top_dir, "install", "sys_include_alias"),
      treemappers.SysIncludeAlias, [modules["glibc_32"]], args=[arch])

  modules["lib32_symlink"] = btarget.TreeMapper(
      "lib32_symlink",
      os.path.join(top_dir, "install", "lib32_symlink"),
      treemappers.Lib32Symlink, [], args=[arch])

  AddAutoconfModule(
      "full-gcc-glibc", "gcc",
      deps=["binutils", "glibc_32", "glibc_64", "installed_linux_headers",
            "dummy_libnacl", "dummy_libcrt_platform",
            "linker_scripts", "sys_include_alias", "lib32_symlink"] + gcc_libs,
      configure_opts=[
          "--disable-libmudflap",
          "--disable-decimal-float",
          "--disable-libssp",
          "--disable-libstdcxx-pch",
          "--enable-shared",
          "--target=%s" % arch,
          "--enable-threads=posix",
          "--enable-tls",
          "--disable-libgomp",
          "--enable-languages=c,c++"],
      configure_env=[
          "CC=gcc",
          "CFLAGS=-Dinhibit_libc -DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5"],
      make_cmd=["make", "all"])

  # TODO(mseaborn): Extend the steps below to run against x86-64
  # nacl-glibc.  Many tests currently pass only for x86-32 nacl-glibc.
  glibc_toolchain_deps = [
      "binutils", "full-gcc-glibc", "glibc_32", "glibc_64",
      "dummy_libcrt_platform", "dummy_libnosys",
      "linker_scripts", "installed_linux_headers",
      "installed_nacl_headers", "lib32_symlink"] + gcc_libs
  AddSconsModule(
      "nacl_libs_glibc",
      deps=glibc_toolchain_deps + ["libnacl_headers"],
      scons_args=["MODE=nacl_extra_sdk", "--nacl_glibc",
                  "extra_sdk_update", "extra_sdk_update_header"],
      libdir="lib32")
  glibc_toolchain = MakeInstallPrefix(
      "glibc_toolchain", deps=glibc_toolchain_deps + ["nacl_libs_glibc"])

  modules["hello_glibc"] = btarget.TestModule(
      "hello_glibc",
      os.path.join(top_dir, "build", "hello_glibc"),
      glibc_toolchain,
      hello_c,
      compiler=["%s-gcc" % arch, "-m32"])
  module_list.append(modules["hello_glibc"])

  AddSconsModule(
      "scons_tests",
      deps=glibc_toolchain_deps + ["nacl_libs_glibc"],
      scons_args=["--nacl_glibc", "small_tests", "-k"])

  # Check that all the Scons tests build, including those that do not
  # yet run successfully.
  AddSconsModule(
      "scons_compile_tests",
      deps=glibc_toolchain_deps + ["nacl_libs_glibc"],
      scons_args=["--nacl_glibc", "MODE=nacl"])

  return module_list


def Main(args):
  root_targets = GetTargets()
  # Use an unbuffered version of stdout.  Python/libc adds buffering
  # to stdout when it is not a tty, but this causes output to be
  # ordered wrongly.  See the PYTHONUNBUFFERED environment variable.
  stream = os.fdopen(os.dup(sys.stdout.fileno()), "w", 0)
  btarget.BuildMain(root_targets, args, stream)


if __name__ == "__main__":
  Main(sys.argv[1:])
