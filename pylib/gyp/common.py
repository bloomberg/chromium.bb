#!/usr/bin/python

import os.path
import re


def BuildFileAndTarget(build_file, target):
  # NOTE: If you just want to split up target into a build_file and target,
  # and you know that target already has a build_file that's been produced by
  # this function, pass '' for build_file.

  # NOTE: rsplit is used to disambiguate the Windows drive letter separator.
  target_split = target.rsplit(':', 1)
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


# re objects used by EncodePOSIXShellArgument.  See IEEE 1003.1 XCU.2.2 at
# http://www.opengroup.org/onlinepubs/009695399/utilities/xcu_chap02.html#tag_02_02
# and the documentation for various shells.

# _quote is a pattern that should match any argument that needs to be quoted
# with double-quotes by EncodePOSIXShellArgument.  It matches the following
# characters appearing anywhere in an argument:
#   \t, \n, space  parameter separators
#   #              comments
#   $              expansions (quoted to always expand within one argument)
#   %              called out by IEEE 1003.1 XCU.2.2
#   &              job control
#   '              quoting
#   (, )           subshell execution
#   *, ?, [        pathname expansion
#   ;              command delimiter
#   <, >, |        redirection
#   =              assignment
#   {, }           brace expansion (bash)
#   ~              tilde expansion
# It also matches the empty string, because "" (or '') is the only way to
# represent an empty string literal argument to a POSIX shell.
#
# This does not match the characters in _escape, because those need to be
# backslash-escaped regardless of whether they appear in a double-quoted
# string.
_quote = re.compile('[\t\n #$%&\'()*;<=>?[{|}~]|^$')

# _escape is a pattern that should match any character that needs to be
# escaped with a backslash, whether or not the argument matched the _quote
# pattern.  _escape is used with re.sub to backslash anything in _escape's
# first match group, hence the (parentheses) in the regular expression.
#
# _escape matches the following characters appearing anywhere in an argument:
#   "  to prevent POSIX shells from interpreting this character for quoting
#   \  to prevent POSIX shells from interpreting this character for escaping
#   `  to prevent POSIX shells from interpreting this character for command
#      substitution
# Missing from this list is $, because the desired behavior of
# EncodePOSIXShellArgument is to permit parameter (variable) expansion.
#
# Also missing from this list is !, which bash will interpret as the history
# expansion character when history is enabled.  bash does not enable history
# by default in non-interactive shells, so this is not thought to be a problem.
# ! was omitted from this list because bash interprets "\!" as a literal string
# including the backslash character (avoiding history expansion but retaining
# the backslash), which would not be correct for argument encoding.  Handling
# this case properly would also be problematic because bash allows the history
# character to be changed with the histchars shell variable.  Fortunately,
# as history is not enabled in non-interactive shells and
# EncodePOSIXShellArgument is only expected to encode for non-interactive
# shells, there is no room for error here by ignoring !.
_escape = re.compile(r'(["\\`])')

def EncodePOSIXShellArgument(argument):
  """Encodes |argument| suitably for consumption by POSIX shells.

  argument may be quoted and escaped as necessary to ensure that POSIX shells
  treat the returned value as a literal representing the argument passed to
  this function.  Parameter (variable) expansions beginning with $ are allowed
  to remain intact without escaping the $, to allow the argument to contain
  references to variables to be expanded by the shell.
  """

  if _quote.search(argument):
    quote = '"'
  else:
    quote = ''

  encoded = quote + re.sub(_escape, r'\\\1', argument) + quote

  return encoded


def EncodePOSIXShellList(list):
  """Encodes |list| suitably for consumption by POSIX shells.

  Returns EncodePOSIXShellArgument for each item in list, and joins them
  together using the space character as an argument separator.
  """

  encoded_arguments = []
  for argument in list:
    encoded_arguments.append(EncodePOSIXShellArgument(argument))
  return ' '.join(encoded_arguments)
