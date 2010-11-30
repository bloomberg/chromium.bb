
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import hashlib
import itertools
import optparse
import os
import re
import subprocess
import sys

import dirtree
import treemappers


def Unrepr(x):
  # TODO(mseaborn): Ideally use a safe eval
  return eval(x, {})


def CheckedRepr(val):
  x = repr(val)
  assert Unrepr(x) == val
  return x


class BuildOptions(object):

  allow_non_pristine = False
  check_for_manual_change = True
  allow_overwrite = False


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


class UnexpectedChangeError(Exception):

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

  def _GetOutputHash(self):
    raise NotImplemented()

  def NeedsBuild(self, opts):
    # TODO(mseaborn): add a test case for this feature or remove it.
    if os.path.exists(self.dest_path + ".pin"):
      return False

    # TODO(mseaborn): Maybe handle the case where output has been
    # manually altered, in which case we don't want to overwrite it
    # (especially if it's source).
    cached = self._state.GetState()
    return (cached is None or
            (not cached.get("pristine", True) and
             not opts.allow_non_pristine) or
            self.GetCurrentInput() != cached["input"])

  def _CheckForManualChange(self, opts):
    cached = self._state.GetState()
    if cached is not None:
      current_hash = self._GetOutputHash()
      if current_hash != cached["output"]:
        msg = ("Build target %r has changed since it was rebuilt "
               "(from %r to %r)." %
               (self.GetName(), cached["output"], current_hash))
        if opts.allow_overwrite:
          sys.stderr.write(msg + "  We are rebuilding it anyway, "
                           "because --allow-overwrite is in force.\n")
        else:
          raise UnexpectedChangeError(
              msg + "  Either it was changed by a badly-behaved build step "
              "or changed by hand, so we are cowardly refusing to overwrite "
              "these changes.  Build with \"%s --allow-overwrite\" to "
              "rebuild the target anyway." % self.GetName())

  def DoBuild(self, opts):
    input = self.GetCurrentInput()
    if opts.check_for_manual_change:
      self._CheckForManualChange(opts)
    result_info = self._build_func(opts)
    if result_info is None:
      result_info = {}
    # TODO(mseaborn): This could record other metadata, such as:
    #  * time taken to build
    #  * inputs that shouldn't make a difference, e.g. filenames, username,
    #    hostname, and other environment.
    # It might also be useful to record failures.
    new_state = {"input": input,
                 "output": self._GetOutputHash(),
                 "pristine": result_info.get("pristine", True)}
    self._state.SetState(new_state)

  def GetOutput(self):
    cached = self._state.GetState()
    if cached is None:
      raise TargetNotBuiltException("Target %r has not been built yet" %
                                    self.GetName())
    return cached["output"]


class HashCache(object):

  """
  This object will hash file contents, and can reuse previously cached
  results based on inode number and mtime.  So that the cache does not
  accumulate unused entries over time, it can resave the cache to
  contain only the entries that were used on the last pass.  This is
  similar to the caching provided by Git's index file.  The cache is
  typically scoped to a specific build target's directory.
  """

  # Use a header line so that we can change the format.
  HEADER_LINE = "hash-list-v1\n"

  def __init__(self, hash_func=dirtree.HashFile):
    self._cache = {}
    self._new_cache = set()
    self._hash_func = hash_func

  def HashFileGivenStat(self, filename, stat_info):
    """
    Returns the hash of the given file.  Takes stat_info (as returned
    by os.stat()/os.lstat()) as an argument so that we do not have to
    do the stat() syscall a second time.
    """
    key = (stat_info.st_dev, stat_info.st_ino, stat_info.st_mtime)
    file_hash = self._cache.get(key)
    if file_hash is None:
      file_hash = self._hash_func(filename)
      self._cache[key] = file_hash
    self._new_cache.add((key, file_hash))
    return file_hash

  def WriteCacheToFile(self, filename):
    fh = open(filename, "w")
    fh.write(self.HEADER_LINE)
    for entry in sorted(self._new_cache):
      fh.write(CheckedRepr(entry) + "\n")
    fh.close()

  def ReadCacheFromFile(self, filename):
    fh = open(filename, "r")
    header = fh.readline()
    # Ignore the file if we do not recognise the format.
    if header == self.HEADER_LINE:
      for line in fh:
        key, file_hash = Unrepr(line)
        self._cache[key] = file_hash
    fh.close()


def ListFiles(tree, dirpath_rel=""):
  # Assumes that directory order does not affect things we build!
  for leafname, child in sorted(tree.iteritems()):
    path_rel = os.path.join(dirpath_rel, leafname)
    if isinstance(child, dirtree.FileSnapshot):
      if child.IsSymlink():
        yield (path_rel, "symlink", child.GetSymlinkDest())
      else:
        yield (path_rel, "file", child.GetHash(), child.IsExecutable())
    else:
      yield (path_rel, "dir")
      for result in ListFiles(child, path_rel):
        yield result


def HashTree(tree):
  # TODO(mseaborn): We could use the same tree hashing as Git so that
  # tree snapshots can be stored in Git without needing a different ID
  # scheme.
  state = hashlib.sha1()
  for info in ListFiles(tree):
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

  def _GetHashCacheFile(self):
    return self.dest_path + ".hashcache"

  def _GetOutputTreeAndCache(self):
    file_hasher = HashCache()
    if os.path.exists(self._GetHashCacheFile()):
      file_hasher.ReadCacheFromFile(self._GetHashCacheFile())
    tree = treemappers.RemoveVersionControlDirs(
        dirtree.MakeSnapshotFromPath(self.dest_path, file_hasher))
    return tree, file_hasher

  def GetOutputTree(self):
    tree, file_hasher = self._GetOutputTreeAndCache()
    return tree

  def _GetOutputHash(self):
    tree, file_hasher = self._GetOutputTreeAndCache()
    # Since HashTree() does a full pass over the tree, the whole tree
    # will be read into the hash cache, so we can save the cache.
    result = HashTree(tree)
    file_hasher.WriteCacheToFile(self._GetHashCacheFile())
    return result


class NonSnapshottingBuildTarget(BuildTarget):

  # This is for source trees that are too big or contain too many
  # constantly-changing files to hash.  This is true for the NaCl
  # source tree because it is also a build tree.
  # TODO(mseaborn): Maybe track subdirs of native_client instead,
  # or regard the tree as always having changed.
  def _GetOutputHash(self):
    return None


def ResetDir(dest_dir):
  dirtree.RemoveTree(dest_dir)
  os.makedirs(dest_dir)


# TODO(mseaborn): This should record the inputs in "args".
# TODO(mseaborn): This should rescan the source tree on each rebuild
# to check whether it has been manually modified.
def SourceTarget(name, dest_dir, src_tree):
  def DoBuild(opts):
    ResetDir(dest_dir)
    src_tree.WriteTree(dest_dir)
  return BuildTarget(name, dest_dir, DoBuild,
                     args=["source", src_tree.GetId()], deps=[])


def SourceTargetGit(name, dest_dir, url, commit_id):
  if re.match("[0-9a-f]{40}$", commit_id) is None:
    raise Exception("Not a canonical Git commit ID: %r" % commit_id)
  def DoBuild(opts):
    # We do "git init+fetch+checkout" as a more incremental,
    # idempotent way of doing "git clone".
    if not os.path.exists(dest_dir):
      os.makedirs(dest_dir)
      subprocess.check_call(["git", "init"], cwd=dest_dir)
      subprocess.check_call(["git", "remote", "add", "origin", url],
                            cwd=dest_dir)
    else:
      # Set the URL again in case it has changed.
      subprocess.check_call(["git", "remote", "set-url", "origin", url],
                            cwd=dest_dir)
    # We only need to run "git fetch" if our repository does not
    # already contain the commit object we need.
    rc = subprocess.call(["git", "cat-file", "-e", commit_id], cwd=dest_dir)
    if rc != 0:
      subprocess.check_call(["git", "fetch", "origin"], cwd=dest_dir)
    subprocess.check_call(["git", "checkout", commit_id], cwd=dest_dir)
  return BuildTarget(name, dest_dir, DoBuild,
                     args=["source_git", commit_id], deps=[])


def ExistingSource(name, dir_path):
  def DoBuild(opts):
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

  def DoBuild(opts):
    prefix_cmd = ["env"] + GetPrefixVars(prefix_obj.dest_path)
    pristine = not opts.allow_non_pristine or not os.path.exists(build_dir)
    if pristine:
      ResetDir(build_dir)
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
    return {"pristine": pristine}

  return BuildTarget(name, install_dir, DoBuild,
                     args=["autoconf", configure_opts, configure_env,
                           make_cmd, install_cmd],
                     deps=[prefix_obj, src] + explicitly_passed_deps)


def ExportHeaders(name, dest_dir, src_dir):
  def DoBuild(opts):
    ResetDir(dest_dir)
    service_runtime_dir = os.path.join(
        src_dir.dest_path, "native_client/src/trusted/service_runtime")
    subprocess.check_call(
        [os.path.join(service_runtime_dir, "export_header.py"),
         os.path.join(service_runtime_dir, "include"),
         dest_dir])
  return BuildTarget(name, dest_dir, DoBuild, args=[], deps=[src_dir])


def SconsBuild(name, dest_dir, build_dir, src_dir, prefix_obj,
               scons_args, arch, libdir="lib"):
  scons_cmd = [
      "./scons",
      "DESTINATION_ROOT=%s" % build_dir,
      "naclsdk_mode=custom:%s" % prefix_obj.dest_path,
      "extra_sdk_lib_destination=%s" % os.path.join(
          dest_dir, arch, libdir),
      "extra_sdk_include_destination=%s" % os.path.join(
          dest_dir, arch, "include"),
      ] + scons_args
  def DoBuild(opts):
    pristine = not opts.allow_non_pristine or not os.path.exists(build_dir)
    if pristine:
      ResetDir(build_dir)
    ResetDir(dest_dir)
    subprocess.check_call(scons_cmd + ["--verbose"],
                          cwd=os.path.join(src_dir.dest_path, "native_client"))
    return {"pristine": pristine}
  return BuildTarget(name, dest_dir, DoBuild,
                     args=[scons_cmd],
                     deps=[src_dir, prefix_obj])


def TreeMapper(name, dest_dir, map_func, input_trees, args=[]):
  def DoBuild(opts):
    trees = [input_tree.GetOutputTree() for input_tree in input_trees]
    result = map_func(*trees + args)
    ResetDir(dest_dir)
    dirtree.WriteSnapshotToPath(result, dest_dir)
  return BuildTarget(name, dest_dir, DoBuild,
                     args=["tree_mapper", map_func.function_identity] + args,
                     deps=input_trees)


def UnionDir(name, dest_dir, input_trees):
  return TreeMapper(name, dest_dir, treemappers.UnionDir, input_trees)


def TestModule(name, dest_dir, prefix_obj, code, compiler):
  def DoBuild(opts):
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


def PrintPlan(targets, stream, opts=BuildOptions()):
  @Memoize
  def Visit(target):
    state = "no"
    for dep in target.GetDeps():
      if Visit(dep) != "no":
        state = "maybe"
    try:
      if target.NeedsBuild(opts):
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


def Rebuild(targets, stream, opts=BuildOptions()):
  @Memoize
  def Visit(target):
    for dep in target.GetDeps():
      Visit(dep)
    if target.NeedsBuild(opts):
      stream.write("** building %s\n" % target.GetName())
      target.DoBuild(opts)
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


# In order to make the Graphviz graph more understandable, we omit
# transitive edges, i.e. any direct dependency that is implied by an
# indirect dependency.  For example, most targets use binutils at
# build time, but showing all those edges turns the graph into
# spaghetti.
#
# However, dropping edges loses information.  For example, both
# pre-gcc and full-gcc depend on gcc-src, but since full-gcc depends
# indirectly on pre-gcc, no edge is shown from full-gcc to gcc-src.
def RemoveTransitiveEdges(graph):
  @Memoize
  def TransitiveClosure(node):
    got = set()
    for dep in graph[node]:
      got.add(dep)
      got.update(TransitiveClosure(dep))
    return got

  graph2 = {}
  for node, deps in graph.iteritems():
    to_remove = set()
    for dep in deps:
      to_remove.update(TransitiveClosure(dep))
    graph2[node] = [dep for dep in deps
                    if dep not in to_remove]
  return graph2


def OutputGraphvizGraph(targets, stream):
  graph = {}
  @Memoize
  def Visit(target):
    graph[target] = target.GetDeps()
    for dep in target.GetDeps():
      Visit(dep)
  for target in targets:
    Visit(target)
  graph = RemoveTransitiveEdges(graph)

  counter = itertools.count()
  @Memoize
  def GetId(target):
    return "n%i" % counter.next()

  stream.write("digraph {\n")
  for node, deps in sorted(graph.iteritems(),
                           key=lambda (node, deps): node.GetName()):
    tid = GetId(node)
    stream.write("  %s [label=\"%s\"];\n" % (tid, node.GetName()))
    for dep in sorted(deps, key=lambda node: node.GetName()):
      stream.write("  %s -> %s;\n" % (tid, GetId(dep)))
  stream.write("}\n")


def BuildMain(root_targets, args, stream):
  parser = optparse.OptionParser("%prog [options] [action] [target...]")
  parser.add_option("-b", "--build", dest="do_build", action="store_true",
                    help="Do the build")
  parser.add_option("-s", "--single", dest="single", action="store_true",
                    help="Build single targets without following "
                    "dependencies.  This will rebuild targets even if they "
                    "are considered up-to-date.")
  parser.add_option("--np", "--non-pristine", dest="non_pristine",
                    action="store_true",
                    help="Re-run 'make' without deleting the build directory "
                    "or re-running 'configure'")
  parser.add_option("--allow-overwrite", dest="allow_overwrite",
                    action="store_true",
                    help="If a target's contents do not match what we "
                    "previously built, allow the target to be clobbered. "
                    "This could lose manual changes to the target.")
  options, args = parser.parse_args(args)

  do_graph = False
  if len(args) > 0 and args[0] == "graph":
    do_graph = True
    args = args[1:]

  opts = BuildOptions()
  opts.allow_non_pristine = options.non_pristine
  opts.allow_overwrite = options.allow_overwrite
  if len(args) > 0:
    root_targets = SubsetTargets(root_targets, args)
  if do_graph:
    OutputGraphvizGraph(root_targets, stream)
  elif options.single:
    if len(args) == 0:
      parser.error("No targets specified with --single")
    for target in root_targets:
      stream.write("%s: will force build\n" % target.GetName())
    if options.do_build:
      for target in root_targets:
        stream.write("** building %s\n" % target.GetName())
        target.DoBuild(opts)
  else:
    PrintPlan(root_targets, stream, opts)
    if options.do_build:
      Rebuild(root_targets, stream, opts)
