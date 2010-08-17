
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


# This is done instead of the lib32 -> lib/32 symlink that Makefile uses.
# TODO(mseaborn): Fix newlib to not output using this odd layout.
def MungeMultilibDir(tree):
  lib32 = tree.get("nacl64", {}).get("lib", {}).get("32")
  if lib32 is not None:
    assert "lib32" not in tree["nacl64"]
    del tree["nacl64"]["lib"]["32"]
    tree["nacl64"]["lib32"] = lib32


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
    MungeMultilibDir(tree)
  combined = UnionDir(*trees)
  AddEnvVarWrapperScripts(combined)
  return combined


def AddHeadersToNewlib(newlib_source, nacl_headers):
  UnionIntoDict(nacl_headers, newlib_source["newlib"]["libc"]["sys"]["nacl"])
  return newlib_source


def InstallLinkerScripts(glibc_tree):
  return {"nacl64": {"lib": glibc_tree["nacl"]["dyn-link"]}}


def InstallKernelHeaders(include_dir_parent):
  return {"nacl64": include_dir_parent}


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
