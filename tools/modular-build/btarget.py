
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import hashlib
import optparse
import os
import stat
import subprocess

import dirtree
import treemappers


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
      raise TargetNotBuiltException("Target %r has not been built yet" %
                                    self.GetName())
    return cached["output"]


def HashFile(filename):
  hasher = hashlib.sha1()
  fh = open(filename, "r")
  try:
    while True:
      data = fh.read(4096)
      if len(data) == 0:
        break
      hasher.update(data)
  finally:
    fh.close()
  return hasher.hexdigest()


def ListFiles(dirpath, dirpath_rel=""):
  # Assumes that directory order does not affect things we build!
  for leafname in sorted(os.listdir(dirpath)):
    path = os.path.join(dirpath, leafname)
    path_rel = os.path.join(dirpath_rel, leafname)
    st = os.lstat(path)
    if stat.S_ISREG(st.st_mode):
      is_executable = st.st_mode & stat.S_IXUSR != 0
      yield (path_rel, "file", HashFile(path), is_executable)
    elif stat.S_ISLNK(st.st_mode):
      # Should we try to record state of things we symlink to?
      yield (path_rel, "symlink", os.readlink(path))
    elif stat.S_ISDIR(st.st_mode):
      yield (path_rel, "dir")
      for result in ListFiles(path, path_rel):
        yield result
    else:
      raise AssertionError("Unknown file type %x: %r" % (st.st_mode, path))


def HashTree(dirpath):
  # TODO(mseaborn): We could use the same tree hashing as Git so that
  # tree snapshots can be stored in Git without needing a different ID
  # scheme.
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
    if not os.path.join(self.dest_path).startswith("/"):
      # Build steps tend to require absolute pathnames because they
      # can be run from various current directories.
      raise AssertionError("Non-absolute pathname: %r" % self.dest_path)

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


def InstallDestdir(prefix_dir, install_dir, func):
  temp_dir = "%s.tmp" % install_dir
  dirtree.RemoveTree(temp_dir)
  func(temp_dir)
  # Tree is installed into $DESTDIR/$prefix.
  # We need to strip $prefix.
  assert prefix_dir.startswith("/")
  temp_subdir = os.path.join(temp_dir, prefix_dir.lstrip("/"))
  dirtree.RemoveTree(install_dir)
  os.rename(temp_subdir, install_dir)
  # TODO: assert that temp_dir doesn't contain anything except prefix dirs
  dirtree.RemoveTree(temp_dir)


def GetPrefixVars(prefix_dir):
  env_vars = []

  def AddVar(var_name, value, separator=":"):
    if var_name in os.environ:
      value = value + separator + os.environ[var_name]
    env_vars.append("%s=%s" % (var_name, value))

  AddVar("PATH", os.path.join(prefix_dir, "bin"))
  AddVar("C_INCLUDE_PATH", os.path.join(prefix_dir, "include"))
  AddVar(treemappers.LibraryPathVar(), os.path.join(prefix_dir, "lib"))
  AddVar("LDFLAGS", "-L%s" % os.path.join(prefix_dir, "lib"),
         separator=" ")
  return env_vars


def AutoconfModule(name, install_dir, build_dir, prefix_obj, src,
                   configure_opts=[], configure_env=[],
                   make_cmd=["make"],
                   install_cmd=["make", "install"],
                   explicitly_passed_deps=[],
                   use_install_root=False):
  prefix_dir = prefix_obj.dest_path
  assert os.path.join(src.dest_path).startswith("/"), src.dest_path

  def DoBuild():
    ResetDir(build_dir)
    prefix_cmd = ["env"] + GetPrefixVars(prefix_obj.dest_path)
    subprocess.check_call(
        prefix_cmd + ["env"] + configure_env +
        [os.path.join(src.dest_path, "configure"),
         "--prefix=%s" % prefix_dir] + configure_opts,
        cwd=build_dir)
    subprocess.check_call(prefix_cmd + make_cmd + ["-j2"], cwd=build_dir)

    if use_install_root:
      install_dir_tmp = install_dir + ".tmp"
      dirtree.RemoveTree(install_dir_tmp)
      subprocess.check_call(
          prefix_cmd + install_cmd + ["install_root=%s" % install_dir_tmp],
          cwd=build_dir)
      dirtree.RemoveTree(install_dir)
      os.rename(install_dir_tmp, install_dir)
    else:
      def DoInstall(dest_dir):
        subprocess.check_call(
            prefix_cmd + install_cmd + ["DESTDIR=%s" % dest_dir],
            cwd=build_dir)
      InstallDestdir(prefix_dir, install_dir, DoInstall)

  return BuildTarget(name, install_dir, DoBuild,
                     args=["autoconf", configure_opts, configure_env,
                           make_cmd, install_cmd],
                     deps=[prefix_obj, src] + explicitly_passed_deps)


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
        ["env"] + GetPrefixVars(prefix_obj.dest_path) +
        ["./scons",
         "USE_ENVIRON=1",
         "naclsdk_mode=custom:%s" % dest_dir,
         "naclsdk_validate=0",
         "--verbose"] + scons_args,
        cwd=src_dir.dest_path)
  return BuildTarget(name, dest_dir, DoBuild,
                     args=[scons_args],
                     deps=[src_dir, prefix_obj])


def TreeMapper(name, dest_dir, map_func, input_trees):
  def DoBuild():
    result = map_func(*[dirtree.MakeSnapshotFromPath(input_tree.dest_path)
                        for input_tree in input_trees])
    ResetDir(dest_dir)
    dirtree.WriteSnapshotToPath(result, dest_dir)
  return BuildTarget(name, dest_dir, DoBuild,
                     args=["tree_mapper", map_func.function_identity],
                     deps=input_trees)


def UnionDir(name, dest_dir, input_trees):
  return TreeMapper(name, dest_dir, treemappers.UnionDir, input_trees)


def TestModule(name, dest_dir, prefix_obj, code, compiler):
  def DoBuild():
    ResetDir(dest_dir)
    dirtree.WriteFile(os.path.join(dest_dir, "prog.c"), code)
    subprocess.check_call(
        ["env"] + GetPrefixVars(prefix_obj.dest_path) +
        compiler +
        [os.path.join(dest_dir, "prog.c"),
         "-o", os.path.join(dest_dir, "prog")])
  return BuildTarget(name, dest_dir, DoBuild, args=[code, compiler],
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


def Rebuild(targets, stream):
  @Memoize
  def Visit(target):
    for dep in target.GetDeps():
      Visit(dep)
    if target.NeedsBuild():
      stream.write("** building %s\n" % target.GetName())
      target.DoBuild()
    else:
      stream.write("** skipping %s\n" % target.GetName())
  for target in targets:
    Visit(target)


def GetTargetNameMapping(root_targets):
  all_targets = {}
  @Memoize
  def Visit(target):
    if target.GetName() in all_targets:
      raise Exception("Multiple targets with name %r" % target.GetName())
    all_targets[target.GetName()] = target
    for dep in target.GetDeps():
      Visit(dep)
  for target in root_targets:
    Visit(target)
  return all_targets


def SubsetTargets(root_targets, names):
  all_targets = GetTargetNameMapping(root_targets)
  return [all_targets[name] for name in names]


def BuildMain(root_targets, args, stream):
  parser = optparse.OptionParser()
  parser.add_option("-b", "--build", dest="do_build", action="store_true",
                    help="Do the build")
  parser.add_option("-s", "--single", dest="single", action="store_true",
                    help="Build single targets without following "
                    "dependencies.  This will rebuild targets even if they "
                    "are considered up-to-date.")
  options, args = parser.parse_args(args)
  if len(args) > 0:
    root_targets = SubsetTargets(root_targets, args)
  if options.single:
    if len(args) == 0:
      parser.error("No targets specified with --single")
    for target in root_targets:
      stream.write("%s: will force build\n" % target.GetName())
    if options.do_build:
      for target in root_targets:
        stream.write("** building %s\n" % target.GetName())
        target.DoBuild()
  else:
    PrintPlan(root_targets, stream)
    if options.do_build:
      Rebuild(root_targets, stream)
