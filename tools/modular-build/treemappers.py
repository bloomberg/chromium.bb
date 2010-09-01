
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import os
import hashlib
import sys
import types

# Do not add any more imports here!  This could lead to undeclared
# dependencies, changes to which fail to trigger rebuilds.
from dirtree import FileSnapshot, FileSnapshotInMemory


def UnionIntoDict(input_tree, dest_dict, context=""):
  for key, value in sorted(input_tree.iteritems()):
    new_context = os.path.join(context, key)
    if isinstance(value, FileSnapshot):
      if key in dest_dict:
        # TODO(mseaborn): Ideally we should pass in a log stream
        # explicitly instead of using sys.stderr.
        sys.stderr.write("Warning: %r is being overwritten\n" % new_context)
      dest_dict[key] = value
    else:
      dest_subdir = dest_dict.setdefault(key, {})
      if isinstance(dest_subdir, FileSnapshot):
        raise Exception("Cannot overwrite directory %r with file"
                        % new_context)
      UnionIntoDict(value, dest_subdir, new_context)


def UnionDir(*trees):
  dest = {}
  for tree in trees:
    UnionIntoDict(tree, dest)
  return dest


def GetPath(tree, path):
  for element in path.split("/"):
    tree = tree[element]
  return tree


def SetPath(tree, path, value):
  elements = path.split("/")
  for element in elements[:-1]:
    tree = tree.setdefault(element, {})
  tree[elements[-1]] = value


# The tree mapper functions below -- AddHeadersToNewlib() and
# InstallLinkerScripts() -- are ad-hoc, in the sense that they support
# specific components that could not be treated uniformly.  They have
# TODOs elsewhere.
#
# If we need more ad-hoc tree mappers, it would make sense to split
# this file up and use a different method for tracking when these
# functions' definitions have changed.  But for now, since there is
# only a small number of them, it is simpler to keep them in one place
# and use the file-based identity tracking method discussed below.


# This is primarily a workaround for the multilib layout that newlib
# produces from "make install".  It outputs to "lib/32", whereas
# everyone else outputs to "lib32", and gcc's search path has "lib32"
# not "lib/32".
# This is done instead of the lib32 -> lib/32 symlink that Makefile uses.
# TODO(mseaborn): Fix newlib to not output using this odd layout.
def MungeMultilibDir(tree, arch, bits):
  libdir = tree.get(arch, {}).get("lib", {}).get(bits)
  if libdir is not None:
    assert "lib" + bits not in tree[arch]
    del tree[arch]["lib"][bits]
    tree[arch]["lib" + bits] = libdir


# This is a workaround for gcc, which installs libgcc_s.so.1 into
# "nacl/lib32" instead of "nacl/lib", even though it is built with
# "--disable-multilib".  However, the Scons build expects libraries to
# be in "lib", so rename "lib32" to "lib".
def RenameLib32ToLib(tree, arch, bits):
  libdir = tree.get(arch, {}).get("lib" + bits)
  if libdir is not None:
    assert ("lib" not in tree[arch] or
            tree[arch]["lib"] == {})
    del tree[arch]["lib" + bits]
    tree[arch]["lib"] = libdir


def LibraryPathVar():
  if sys.platform == "darwin":
    return "DYLD_LIBRARY_PATH"
  else:
    return "LD_LIBRARY_PATH"


def AddEnvVarWrapperScripts(tree):
  # Move the real executables from "bin" to "original-bin" and create
  # wrapper scripts in "bin" that set LD_LIBRARY_PATH.
  if "bin" in tree:
    assert "original-bin" not in tree
    tree["original-bin"] = tree["bin"]
    tree["bin"] = {}
    for script_name in tree["original-bin"].iterkeys():
      template = """\
#!/bin/bash
export %(path_var)s="${0%%/*}/../lib${%(path_var)s+:$%(path_var)s}"
exec ${0%%/*}/../original-bin/%(script_name)s "$@"
"""
      script = template % {"path_var": LibraryPathVar(),
                           "script_name": script_name}
      tree["bin"][script_name] = FileSnapshotInMemory(script, executable=True)


def CombineInstallTrees(*trees):
  for tree in trees:
    MungeMultilibDir(tree, "nacl64", "32")
    MungeMultilibDir(tree, "nacl", "64")
    RenameLib32ToLib(tree, "nacl", "32")
  combined = UnionDir(*trees)
  AddEnvVarWrapperScripts(combined)
  return combined


def AddHeadersToNewlib(newlib_source, nacl_headers):
  UnionIntoDict(nacl_headers, newlib_source["newlib"]["libc"]["sys"]["nacl"])
  return newlib_source


def InstallLinkerScripts(glibc_tree, arch):
  return {arch: {"lib": glibc_tree["nacl"]["dyn-link"]}}


def InstallKernelHeaders(include_dir_parent, arch):
  return {arch: include_dir_parent}


def SubsetNaClHeaders(input_headers, arch):
  # We install only a subset of the NaCl headers from
  # service_runtime/include.  We don't want the headers for POSIX
  # interfaces that are provided by glibc, e.g. sys/mman.h.
  result = {}
  headers = [
      "sys/nacl_imc_api.h",
      "bits/nacl_imc_api.h",
      "sys/nacl_syscalls.h",
      "sys/audio_video.h",
      "machine/_types.h"]
  for filename in headers:
    SetPath(result, filename, GetPath(input_headers, filename))
  # TODO(mseaborn): _default_types.h is a newlibism.  We should remove
  # dependencies on it.  Its definitions are supplied by other headers
  # in glibc.
  SetPath(result, "machine/_default_types.h",
          FileSnapshotInMemory("/* Intentionally empty */\n"))
  return {arch: {"include": result}}


def CreateAlias(new_name, old_name):
  template = """\
#!/bin/sh
exec %s "$@"
"""
  return {"bin": {new_name: FileSnapshotInMemory(template % old_name,
                                                 executable=True)}}


def DummyLibs(arch):
  # This text file works as a dummy (empty) library because ld treats
  # it as a linker script.
  dummy_lib = FileSnapshotInMemory("/* Intentionally empty */\n")
  return {arch: {"lib": {"libnacl.so": dummy_lib,
                         "libcrt_platform.so": dummy_lib}}}


# When gcc is built, it checks whether libc provides <limits.h> in
# order to determine whether gcc's own <limits.h> should use libc's
# version via #include_next.  Oddly, gcc looks in "sys-include" rather
# than "include".  We work around this by creating "sys-include" as an
# alias.  See http://code.google.com/p/nativeclient/issues/detail?id=854
def SysIncludeAlias(tree, arch):
  return {arch: {"sys-include": tree[arch]["include"]}}


# The functions above are fairly cheap, so we could run them each
# time, but they do require scanning their input directory trees, so
# it would be better to avoid that if the function has not changed.
#
# To do that, we need a way to serialise the identities of the
# functions.  We do that just by recording the hash of this file.
#
# More complex approaches could be:
#  * Hashing a function's Python bytecode.  This captures line numbers
#    for debugging so it would still change on trivial changes.
#  * Hashing a function's AST.

def MarkFunctionsWithIdentity(module):
  filename = __file__
  if filename.endswith(".pyc"):
    # Change ".pyc" extension to ".py".
    filename = filename[:-1]
  fh = open(filename, "r")
  module_identity = hashlib.sha1(fh.read()).hexdigest()
  fh.close()
  for value in module.itervalues():
    if isinstance(value, types.FunctionType):
      value.function_identity = (module_identity, __name__)


MarkFunctionsWithIdentity(globals())
