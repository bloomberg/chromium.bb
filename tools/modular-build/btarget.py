
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import hashlib
import os
import stat
import subprocess

import dirtree


def Unrepr(x):
  # TODO(mseaborn): Ideally use a safe eval
  return eval(x, {})


def CheckedRepr(val):
  x = repr(val)
  assert Unrepr(x) == val
  return x


class TargetState(object):

  def __init__(self, filename):
    self._filename = filename

  def GetState(self, default=None):
    if os.path.exists(self._filename):
      return Unrepr(dirtree.ReadFile(self._filename))
    else:
      return default

  def SetState(self, val):
    x = CheckedRepr(val)
    dirtree.WriteFile(self._filename, x)
    fh = open("%s.log" % self._filename, "a")
    fh.write(x + "\n")
    fh.close()


class TargetNotBuiltException(Exception):

  pass


class TargetBase(object):

  def GetDeps(self):
    raise NotImplemented()

  def GetName(self):
    raise NotImplemented()

  def GetCurrentInput(self):
    return {"deps": dict((dep.GetName(), dep.GetOutput())
                         for dep in self.GetDeps()),
            "args": self._args}

  def _GetOutputSnapshot(self):
    raise NotImplemented()

  def NeedsBuild(self):
    # TODO(mseaborn): add a test case for this feature or remove it.
    if os.path.exists(self.dest_path + ".pin"):
      return False

    # TODO(mseaborn): Maybe handle the case where output has been
    # manually altered, in which case we don't want to overwrite it
    # (especially if it's source).
    cached = self._state.GetState()
    return cached is None or self.GetCurrentInput() != cached["input"]

  def DoBuild(self):
    input = self.GetCurrentInput()
    self._build_func()
    # TODO(mseaborn): This could record other metadata, such as:
    #  * time taken to build
    #  * whether this was a pristine build
    #  * inputs that shouldn't make a difference, e.g. filenames, username,
    #    hostname, and other environment.
    # It might also be useful to record failures.
    self._state.SetState({"input": input,
                          "output": self._GetOutputSnapshot()})

  def GetOutput(self):
    cached = self._state.GetState()
    if cached is None:
      raise TargetNotBuiltException()
    return cached["output"]


def ListFiles(dirpath, dirpath_rel=""):
  # Assumes that directory order does not affect things we build!
  for leafname in sorted(os.listdir(dirpath)):
    path = os.path.join(dirpath, leafname)
    path_rel = os.path.join(dirpath_rel, leafname)
    st = os.lstat(path)
    yield path_rel, st.st_mode, st.st_mtime, st.st_dev, st.st_ino
    # TODO(mseaborn): S_ISLNK: record link destination.
    # Do we try to record state of things we symlink to?
    if stat.S_ISDIR(st.st_mode):
      for result in ListFiles(path, path_rel):
        yield result


def HashTree(dirpath):
  # TODO(mseaborn): Use the same tree hashing as, say, Git, so that we
  # handle empty directories and file execute permission.
  state = hashlib.sha1()
  for info in ListFiles(dirpath):
    state.update(CheckedRepr(info) + "\n")
  return state.hexdigest()


class BuildTarget(TargetBase):

  def __init__(self, name, dest_dir, func, args, deps):
    self._name = name
    self.dest_path = dest_dir
    self._build_func = func
    self._args = args
    self._deps = deps
    self._state = TargetState("%s.state" % dest_dir)

  def GetDeps(self):
    return self._deps

  def GetName(self):
    return self._name

  def _GetOutputSnapshot(self):
    return HashTree(self.dest_path)


class NonSnapshottingBuildTarget(BuildTarget):

  # This is for source trees that are too big or contain too many
  # constantly-changing files to hash.  This is true for the NaCl
  # source tree because it is also a build tree.
  # TODO(mseaborn): Maybe track subdirs of native_client instead,
  # or regard the tree as always having changed.
  def _GetOutputSnapshot(self):
    return None


def ResetDir(dest_dir):
  dirtree.RemoveTree(dest_dir)
  os.makedirs(dest_dir)


# TODO(mseaborn): This should record the inputs in "args".
# TODO(mseaborn): This should rescan the source tree on each rebuild
# to check whether it has been manually modified.
def SourceTarget(name, dest_dir, src_tree):
  def DoBuild():
    ResetDir(dest_dir)
    src_tree.WriteTree(dest_dir)
  return BuildTarget(name, dest_dir, DoBuild, args=["source"], deps=[])


def ExistingSource(name, dir_path):
  def DoBuild():
    pass
  return NonSnapshottingBuildTarget(name, dir_path, DoBuild,
                                    args=["existing"], deps=[])


def MungeMultilibDir(install_dir):
  # This is done instead of the lib32 -> lib/32 symlink that Makefile uses.
  # TODO(mseaborn): Fix newlib to not output using this odd layout,
  # or move this fixup into build.py.
  if os.path.exists(os.path.join(install_dir, "nacl64/lib/32")):
    os.rename(os.path.join(install_dir, "nacl64/lib/32"),
              os.path.join(install_dir, "nacl64/lib32"))


def InstallDestdir(prefix_dir, install_dir, func):
  temp_dir = "%s.tmp" % install_dir
  dirtree.RemoveTree(temp_dir)
  func(temp_dir)
  dirtree.RemoveTree(install_dir)
  # Tree is installed into $DESTDIR/$prefix.
  # We need to strip $prefix.
  assert prefix_dir.startswith("/")
  temp_subdir = os.path.join(temp_dir, prefix_dir.lstrip("/"))
  MungeMultilibDir(temp_subdir)
  os.rename(temp_subdir, install_dir)
  # TODO: assert that temp_dir doesn't contain anything except prefix dirs
  dirtree.RemoveTree(temp_dir)


def GetPathVar(prefix_obj):
  return "PATH=%s:%s" % (os.path.join(prefix_obj.dest_path, "bin"),
                         os.environ["PATH"])


def AutoconfModule(name, install_dir, build_dir, prefix_obj, src,
                   configure_opts=[], configure_env=[],
                   make_cmd=["make"],
                   install_cmd=["make", "install"]):
  prefix_dir = prefix_obj.dest_path
  assert os.path.join(src.dest_path).startswith("/"), src.dest_path
  def DoBuild():
    ResetDir(install_dir)
    ResetDir(build_dir)
    prefix_cmd = ["env", GetPathVar(prefix_obj)]
    subprocess.check_call(
        prefix_cmd + ["env"] + configure_env +
        [os.path.join(src.dest_path, "configure"),
         "--prefix=%s" % prefix_dir] + configure_opts,
        cwd=build_dir)
    subprocess.check_call(prefix_cmd + make_cmd + ["-j2"], cwd=build_dir)
    def DoInstall(dest_dir):
      subprocess.check_call(
          prefix_cmd + install_cmd + ["DESTDIR=%s" % dest_dir],
          cwd=build_dir)
    InstallDestdir(prefix_dir, install_dir, DoInstall)
  return BuildTarget(name, install_dir, DoBuild,
                     args=["autoconf", configure_opts, configure_env,
                           make_cmd, install_cmd],
                     deps=[prefix_obj, src])


def ExportHeaders(name, dest_dir, src_dir):
  def DoBuild():
    ResetDir(dest_dir)
    nacl_dir = src_dir.dest_path
    subprocess.check_call(
        [os.path.join(nacl_dir,
                      "src/trusted/service_runtime/export_header.py"),
         os.path.join(nacl_dir, "src/trusted/service_runtime/include"),
         dest_dir])
  return BuildTarget(name, dest_dir, DoBuild, args=[], deps=[src_dir])


def SconsBuild(name, dest_dir, src_dir, prefix_obj, scons_args):
  def DoBuild():
    ResetDir(dest_dir)
    # We use naclsdk_mode to point to an empty destination directory
    # so Scons can't get nacl-gcc from there.  We set USE_ENVIRON=1 to
    # get Scons to pass PATH through so that it gets nacl-gcc from
    # PATH.
    # TODO(mseaborn): Get Scons to use a pristine build directory,
    # instead of reusing the contents of "scons-out".
    subprocess.check_call(
        ["env", GetPathVar(prefix_obj),
         "./scons",
         "USE_ENVIRON=1",
         "naclsdk_mode=custom:%s" % dest_dir,
         "naclsdk_validate=0",
         "--verbose"] + scons_args,
        cwd=src_dir.dest_path)
    MungeMultilibDir(dest_dir)
  return BuildTarget(name, dest_dir, DoBuild,
                     args=[scons_args],
                     deps=[src_dir, prefix_obj])


# TODO(mseaborn): Hard link the files instead of copying, because
# having multiple copies is relatively expensive.
def UnionDir(name, dest_dir, input_trees):
  def DoBuild():
    ResetDir(dest_dir)
    for tree in input_trees:
      dirtree.CopyOntoHardlink(tree.dest_path, dest_dir)
  return BuildTarget(name, dest_dir, DoBuild,
                     args=["union_dir"], deps=input_trees)


# Combines input directories under different subdirs.
def UnionDir2(name, dest_dir, input_trees):
  def DoBuild():
    ResetDir(dest_dir)
    for subdir, tree in input_trees:
      dest_subdir = os.path.join(dest_dir, subdir)
      dirtree.MkdirP(dest_subdir)
      dirtree.CopyOntoHardlink(tree.dest_path, dest_subdir)
  return BuildTarget(name, dest_dir, DoBuild,
                     args=["union_dir2"] +
                          [subdir for subdir, tree in input_trees],
                     deps=[tree for subdir, tree in input_trees])


def TestModule(name, dest_dir, prefix_obj, code):
  def DoBuild():
    ResetDir(dest_dir)
    dirtree.WriteFile(os.path.join(dest_dir, "prog.c"), code)
    subprocess.check_call(
        ["env", GetPathVar(prefix_obj)] +
        ["nacl64-gcc", "-m32",
         os.path.join(dest_dir, "prog.c"),
         "-o", os.path.join(dest_dir, "prog")])
  return BuildTarget(name, dest_dir, DoBuild, args=[code],
                     deps=[prefix_obj])


NOT_FOUND = object()


def Memoize(func):
  cache = {}
  def wrapper(*args):
    value = cache.get(args, NOT_FOUND)
    if value is NOT_FOUND:
      value = func(*args)
      cache[args] = value
    return value
  return wrapper


def PrintPlan(targets, stream):
  @Memoize
  def Visit(target):
    state = "no"
    for dep in target.GetDeps():
      if Visit(dep) != "no":
        state = "maybe"
    try:
      if target.NeedsBuild():
        state = "yes"
    except TargetNotBuiltException:
      # This target has been built, but one of its inputs hasn't
      # been built (the input might have been added to the
      # target, or built earlier but deleted), so we don't know
      # if target will need rebuilding.
      state = "maybe"
    stream.write("%s: %s\n" % (target.GetName(), state))
    return state
  for target in targets:
    Visit(target)


def Rebuild(targets):
  @Memoize
  def Visit(target):
    for dep in target.GetDeps():
      Visit(dep)
    if target.NeedsBuild():
      print "** building %s" % target.GetName()
      target.DoBuild()
    else:
      print "** skipping %s" % target.GetName()
  for target in targets:
    Visit(target)
