#!/usr/bin/python

import os.path


def BuildFileAndTarget(build_file, target):
  # NOTE: If you just want to split up target into a build_file and target,
  # and you know that target already has a build_file that's been produced by
  # this function, pass '' for build_file.

  target_split = target.split(':', 1)
  if len(target_split) == 2:
    [build_file_rel, target] = target_split

    # If a relative path, build_file_rel is relative to the directory
    # containing build_file.  If build_file is not in the current directory,
    # build_file_rel is not a usable path as-is.  Resolve it by interpreting it
    # as relative to build_file.  If build_file_rel is absolute, it is usable
    # as a path regardless of the current directory, and os.path.join will
    # return it as-is.
    build_file = os.path.normpath(os.path.join(os.path.dirname(build_file),
                                               build_file_rel))

  return [build_file, target, build_file + ':' + target]


def QualifiedTarget(build_file, target):
  # "Qualified" means the file that a target was defined in and the target
  # name, separated by a colon.
  return BuildFileAndTarget(build_file, target)[2]


def RelativePath(path, relative_to):
  # Assuming both |path| and |relative_to| are relative to the current
  # directory, returns a relative path that identifies path relative to
  # relative_to.

  if os.path.isabs(path) != os.path.isabs(relative_to):
    # If one of the paths is absolute, both need to be absolute.
    path = os.path.abspath(path)
    relative_to = os.path.abspath(relative_to)
  else:
    # If both paths are relative, make sure they're normalized.
    path = os.path.normpath(path)
    relative_to = os.path.normpath(relative_to)

  # Split the paths into components.  As a special case, if either path is
  # the current directory, use an empty list as a split-up path.  This must be
  # done because the code that follows is unprepared to deal with "." meaning
  # "current directory" and it will instead assume that it's a subdirectory,
  # which is wrong.  It's possible to wind up with "." when it's passed to this
  # function, for example, by taking the dirname of a relative path in the
  # current directory.
  if path == os.path.curdir:
    path_split = []
  else:
    path_split = path.split(os.path.sep)

  if relative_to == os.path.curdir:
    relative_to_split = []
  else:
    relative_to_split = relative_to.split(os.path.sep)

  # Determine how much of the prefix the two paths share.
  prefix_len = len(os.path.commonprefix([path_split, relative_to_split]))

  # Put enough ".." components to back up out of relative_to to the common
  # prefix, and then append the part of path_split after the common prefix.
  relative_split = [os.path.pardir] * (len(relative_to_split) - prefix_len) + \
                   path_split[prefix_len:]

  # Turn it back into a string and we're done.
  return os.path.join(*relative_split)
