#!/usr/bin/python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enables directory-specific presubmit checks to run at upload and/or commit.
"""

__version__ = '1.0.1'

# TODO(joi) Add caching where appropriate/needed. The API is designed to allow
# caching (between all different invocations of presubmit scripts for a given
# change). We should add it as our presubmit scripts start feeling slow.

import cPickle  # Exposed through the API.
import cStringIO  # Exposed through the API.
import exceptions
import fnmatch
import glob
import marshal  # Exposed through the API.
import optparse
import os  # Somewhat exposed through the API.
import pickle  # Exposed through the API.
import re  # Exposed through the API.
import subprocess  # Exposed through the API.
import sys  # Parts exposed through API.
import tempfile  # Exposed through the API.
import types
import urllib2  # Exposed through the API.

# Local imports.
# TODO(joi) Would be cleaner to factor out utils in gcl to separate module, but
# for now it would only be a couple of functions so hardly worth it.
import gcl
import gclient
import presubmit_canned_checks


# Matches key/value (or "tag") lines in changelist descriptions.
_tag_line_re = re.compile(
    '^\s*(?P<key>[A-Z][A-Z_0-9]*)\s*=\s*(?P<value>.*?)\s*$')


# Friendly names may be used for certain keys.  All values for key-value pairs
# in change descriptions (like BUG=123) can be retrieved from a change object
# directly as if they were attributes, e.g. change.R (or equivalently because
# we have a friendly name for it, change.Reviewers), change.BUG (or
# change.BugIDs) and so forth.
#
# Add to this mapping as needed/desired.
SPECIAL_KEYS = {
  'Reviewers' : 'R',
  'BugIDs' : 'BUG',
  'Tested': 'TESTED',
  'Test': 'TEST'
}


class NotImplementedException(Exception):
  """We're leaving placeholders in a bunch of places to remind us of the
  design of the API, but we have not implemented all of it yet. Implement as
  the need arises.
  """
  pass


def normpath(path):
  '''Version of os.path.normpath that also changes backward slashes to
  forward slashes when not running on Windows.
  '''
  # This is safe to always do because the Windows version of os.path.normpath
  # will replace forward slashes with backward slashes.
  path = path.replace(os.sep, '/')
  return os.path.normpath(path)



class OutputApi(object):
  """This class (more like a module) gets passed to presubmit scripts so that
  they can specify various types of results.
  """

  class PresubmitResult(object):
    """Base class for result objects."""

    def __init__(self, message, items=None, long_text=''):
      """
      message: A short one-line message to indicate errors.
      items: A list of short strings to indicate where errors occurred.
      long_text: multi-line text output, e.g. from another tool
      """
      self._message = message
      self._items = []
      if items:
        self._items = items
      self._long_text = long_text.rstrip()

    def _Handle(self, output_stream, input_stream, may_prompt=True):
      """Writes this result to the output stream.

      Args:
        output_stream: Where to write

      Returns:
        True if execution may continue, False otherwise.
      """
      output_stream.write(self._message)
      output_stream.write('\n')
      for item in self._items:
        output_stream.write('  %s\n' % item)
      if self._long_text:
        output_stream.write('\n***************\n%s\n***************\n\n' %
                            self._long_text)

      if self.ShouldPrompt() and may_prompt:
        output_stream.write('Are you sure you want to continue? (y/N): ')
        response = input_stream.readline()
        if response.strip().lower() != 'y':
          return False

      return not self.IsFatal()

    def IsFatal(self):
      """An error that is fatal stops g4 mail/submit immediately, i.e. before
      other presubmit scripts are run.
      """
      return False

    def ShouldPrompt(self):
      """Whether this presubmit result should result in a prompt warning."""
      return False

  class PresubmitError(PresubmitResult):
    """A hard presubmit error."""
    def IsFatal(self):
      return True

  class PresubmitPromptWarning(PresubmitResult):
    """An warning that prompts the user if they want to continue."""
    def ShouldPrompt(self):
      return True

  class PresubmitNotifyResult(PresubmitResult):
    """Just print something to the screen -- but it's not even a warning."""
    pass

  class MailTextResult(PresubmitResult):
    """A warning that should be included in the review request email."""
    def __init__(self, *args, **kwargs):
      raise NotImplementedException()  # TODO(joi) Implement.


class InputApi(object):
  """An instance of this object is passed to presubmit scripts so they can
  know stuff about the change they're looking at.
  """

  def __init__(self, change, presubmit_path):
    """Builds an InputApi object.

    Args:
      change: A presubmit.GclChange object.
      presubmit_path: The path to the presubmit script being processed.
    """
    self.change = change

    # We expose various modules and functions as attributes of the input_api
    # so that presubmit scripts don't have to import them.
    self.basename = os.path.basename
    self.cPickle = cPickle
    self.cStringIO = cStringIO
    self.os_path = os.path
    self.pickle = pickle
    self.marshal = marshal
    self.re = re
    self.subprocess = subprocess
    self.tempfile = tempfile
    self.urllib2 = urllib2

    # InputApi.platform is the platform you're currently running on.
    self.platform = sys.platform

    # The local path of the currently-being-processed presubmit script.
    self._current_presubmit_path = os.path.dirname(presubmit_path)

    # We carry the canned checks so presubmit scripts can easily use them.
    self.canned_checks = presubmit_canned_checks

  def PresubmitLocalPath(self):
    """Returns the local path of the presubmit script currently being run.

    This is useful if you don't want to hard-code absolute paths in the
    presubmit script.  For example, It can be used to find another file
    relative to the PRESUBMIT.py script, so the whole tree can be branched and
    the presubmit script still works, without editing its content.
    """
    return self._current_presubmit_path

  @staticmethod
  def DepotToLocalPath(depot_path):
    """Translate a depot path to a local path (relative to client root).

    Args:
      Depot path as a string.

    Returns:
      The local path of the depot path under the user's current client, or None
      if the file is not mapped.

      Remember to check for the None case and show an appropriate error!
    """
    local_path = gclient.CaptureSVNInfo(depot_path).get('Path')
    if not local_path:
      return None
    else:
      return local_path

  @staticmethod
  def LocalToDepotPath(local_path):
    """Translate a local path to a depot path.

    Args:
      Local path (relative to current directory, or absolute) as a string.

    Returns:
      The depot path (SVN URL) of the file if mapped, otherwise None.
    """
    depot_path = gclient.CaptureSVNInfo(local_path).get('URL')
    if not depot_path:
      return None
    else:
      return depot_path

  @staticmethod
  def FilterTextFiles(affected_files, include_deletes=True):
    """Filters out all except text files and optionally also filters out
    deleted files.

    Args:
      affected_files: List of AffectedFiles objects.
      include_deletes: If false, deleted files will be filtered out.

    Returns:
      Filtered list of AffectedFiles objects.
    """
    output_files = []
    for af in affected_files:
      if include_deletes or af.Action() != 'D':
        path = af.AbsoluteLocalPath()
        mime_type = gcl.GetSVNFileProperty(path, 'svn:mime-type')
        if not mime_type or mime_type.startswith('text/'):
          output_files.append(af)
    return output_files

  def AffectedFiles(self, include_dirs=False, include_deletes=True):
    """Same as input_api.change.AffectedFiles() except only lists files
    (and optionally directories) in the same directory as the current presubmit
    script, or subdirectories thereof.
    """
    output_files = []
    dir_with_slash = normpath("%s/" % self.PresubmitLocalPath())
    if len(dir_with_slash) == 1:
      dir_with_slash = ''
    for af in self.change.AffectedFiles(include_dirs, include_deletes):
      af_path = normpath(af.LocalPath())
      if af_path.startswith(dir_with_slash):
        output_files.append(af)
    return output_files

  def LocalPaths(self, include_dirs=False):
    """Returns local paths of input_api.AffectedFiles()."""
    return [af.LocalPath() for af in self.AffectedFiles(include_dirs)]

  def AbsoluteLocalPaths(self, include_dirs=False):
    """Returns absolute local paths of input_api.AffectedFiles()."""
    return [af.AbsoluteLocalPath() for af in self.AffectedFiles(include_dirs)]

  def ServerPaths(self, include_dirs=False):
    """Returns server paths of input_api.AffectedFiles()."""
    return [af.ServerPath() for af in self.AffectedFiles(include_dirs)]

  def AffectedTextFiles(self, include_deletes=True):
    """Same as input_api.change.AffectedTextFiles() except only lists files
    in the same directory as the current presubmit script, or subdirectories
    thereof.

    Warning: This function retrieves the svn property on each file so it can be
    slow for large change lists.
    """
    return InputApi.FilterTextFiles(self.AffectedFiles(include_dirs=False),
                                    include_deletes)

  def RightHandSideLines(self):
    """An iterator over all text lines in "new" version of changed files.

    Only lists lines from new or modified text files in the change that are
    contained by the directory of the currently executing presubmit script.

    This is useful for doing line-by-line regex checks, like checking for
    trailing whitespace.

    Yields:
      a 3 tuple:
        the AffectedFile instance of the current file;
        integer line number (1-based); and
        the contents of the line as a string.
    """
    return InputApi._RightHandSideLinesImpl(
        self.AffectedTextFiles(include_deletes=False))

  @staticmethod
  def _RightHandSideLinesImpl(affected_files):
    """Implements RightHandSideLines for InputApi and GclChange."""
    for af in affected_files:
      lines = af.NewContents()
      line_number = 0
      for line in lines:
        line_number += 1
        yield (af, line_number, line)


class AffectedFile(object):
  """Representation of a file in a change."""

  def __init__(self, path, action, repository_root=''):
    self.path = path
    self.action = action.strip()
    self.repository_root = repository_root

  def ServerPath(self):
    """Returns a path string that identifies the file in the SCM system.

    Returns the empty string if the file does not exist in SCM.
    """
    return gclient.CaptureSVNInfo(self.AbsoluteLocalPath()).get('URL', '')

  def LocalPath(self):
    """Returns the path of this file on the local disk relative to client root.
    """
    return normpath(self.path)

  def AbsoluteLocalPath(self):
    """Returns the absolute path of this file on the local disk.
    """
    return normpath(os.path.join(self.repository_root, self.LocalPath()))

  def IsDirectory(self):
    """Returns true if this object is a directory."""
    path = self.AbsoluteLocalPath()
    if os.path.exists(path):
      # Retrieve directly from the file system; it is much faster than querying
      # subversion, especially on Windows.
      return os.path.isdir(path)
    else:
      return gclient.CaptureSVNInfo(path).get('Node Kind') in ('dir',
                                                               'directory')

  def SvnProperty(self, property_name):
    """Returns the specified SVN property of this file, or the empty string
    if no such property.
    """
    return gcl.GetSVNFileProperty(self.AbsoluteLocalPath(), property_name)

  def Action(self):
    """Returns the action on this opened file, e.g. A, M, D, etc."""
    return self.action

  def NewContents(self):
    """Returns an iterator over the lines in the new version of file.

    The new version is the file in the user's workspace, i.e. the "right hand
    side".

    Contents will be empty if the file is a directory or does not exist.
    """
    if self.IsDirectory():
      return []
    else:
      return gcl.ReadFile(self.AbsoluteLocalPath()).splitlines()

  def OldContents(self):
    """Returns an iterator over the lines in the old version of file.

    The old version is the file in depot, i.e. the "left hand side".
    """
    raise NotImplementedError()  # Implement when needed

  def OldFileTempPath(self):
    """Returns the path on local disk where the old contents resides.

    The old version is the file in depot, i.e. the "left hand side".
    This is a read-only cached copy of the old contents. *DO NOT* try to
    modify this file.
    """
    raise NotImplementedError()  # Implement if/when needed.


class GclChange(object):
  """A gcl change. See gcl.ChangeInfo for more info."""

  def __init__(self, change_info, repository_root=''):
    self.name = change_info.name
    self.full_description = change_info.description
    self.repository_root = repository_root

    # From the description text, build up a dictionary of key/value pairs
    # plus the description minus all key/value or "tag" lines.
    self.description_without_tags = []
    self.tags = {}
    for line in change_info.description.splitlines():
      m = _tag_line_re.match(line)
      if m:
        self.tags[m.group('key')] = m.group('value')
      else:
        self.description_without_tags.append(line)

    # Change back to text and remove whitespace at end.
    self.description_without_tags = '\n'.join(self.description_without_tags)
    self.description_without_tags = self.description_without_tags.rstrip()

    self.affected_files = [AffectedFile(info[1], info[0], repository_root) for
                           info in change_info.files]

  def Change(self):
    """Returns the change name."""
    return self.name

  def Changelist(self):
    """Synonym for Change()."""
    return self.Change()

  def DescriptionText(self):
    """Returns the user-entered changelist description, minus tags.

    Any line in the user-provided description starting with e.g. "FOO="
    (whitespace permitted before and around) is considered a tag line.  Such
    lines are stripped out of the description this function returns.
    """
    return self.description_without_tags

  def FullDescriptionText(self):
    """Returns the complete changelist description including tags."""
    return self.full_description

  def RepositoryRoot(self):
    """Returns the repository root for this change, as an absolute path."""
    return self.repository_root

  def __getattr__(self, attr):
    """Return keys directly as attributes on the object.

    You may use a friendly name (from SPECIAL_KEYS) or the actual name of
    the key.
    """
    if attr in SPECIAL_KEYS:
      key = SPECIAL_KEYS[attr]
      if key in self.tags:
        return self.tags[key]
    if attr in self.tags:
      return self.tags[attr]

  def AffectedFiles(self, include_dirs=False, include_deletes=True):
    """Returns a list of AffectedFile instances for all files in the change.

    Args:
      include_deletes: If false, deleted files will be filtered out.
      include_dirs: True to include directories in the list

    Returns:
      [AffectedFile(path, action), AffectedFile(path, action)]
    """
    if include_dirs:
      affected = self.affected_files
    else:
      affected = filter(lambda x: not x.IsDirectory(), self.affected_files)

    if include_deletes:
      return affected
    else:
      return filter(lambda x: x.Action() != 'D', affected)

  def AffectedTextFiles(self, include_deletes=True):
    """Return a list of the text files in a change.

    It's common to want to iterate over only the text files.

    Args:
      include_deletes: Controls whether to return files with "delete" actions,
      which commonly aren't relevant to presubmit scripts.
    """
    return InputApi.FilterTextFiles(self.AffectedFiles(include_dirs=False),
                                    include_deletes)

  def LocalPaths(self, include_dirs=False):
    """Convenience function."""
    return [af.LocalPath() for af in self.AffectedFiles(include_dirs)]

  def AbsoluteLocalPaths(self, include_dirs=False):
    """Convenience function."""
    return [af.AbsoluteLocalPath() for af in self.AffectedFiles(include_dirs)]

  def ServerPaths(self, include_dirs=False):
    """Convenience function."""
    return [af.ServerPath() for af in self.AffectedFiles(include_dirs)]

  def RightHandSideLines(self):
    """An iterator over all text lines in "new" version of changed files.

    Lists lines from new or modified text files in the change.

    This is useful for doing line-by-line regex checks, like checking for
    trailing whitespace.

    Yields:
      a 3 tuple:
        the AffectedFile instance of the current file;
        integer line number (1-based); and
        the contents of the line as a string.
    """
    return InputApi._RightHandSideLinesImpl(
        self.AffectedTextFiles(include_deletes=False))


def ListRelevantPresubmitFiles(files):
  """Finds all presubmit files that apply to a given set of source files.

  Args:
    files: An iterable container containing file paths.

  Return:
    ['foo/blat/PRESUBMIT.py', 'mat/gat/PRESUBMIT.py']
  """
  checked_dirs = {}  # Keys are directory paths, values are ignored.
  source_dirs = [os.path.dirname(f) for f in files]
  presubmit_files = []
  for dir in source_dirs:
    while (True):
      if dir in checked_dirs:
        break  # We've already walked up from this directory.

      test_path = os.path.join(dir, 'PRESUBMIT.py')
      if os.path.isfile(test_path):
        presubmit_files.append(normpath(test_path))

      checked_dirs[dir] = ''
      if dir in ['', '.']:
        break
      else:
        dir = os.path.dirname(dir)
  return presubmit_files


class PresubmitExecuter(object):

  def __init__(self, change_info, committing):
    """
    Args:
      change_info: The ChangeInfo object for the change.
      committing: True if 'gcl commit' is running, False if 'gcl upload' is.
    """
    self.change = GclChange(change_info, gcl.GetRepositoryRoot())
    self.committing = committing

  def ExecPresubmitScript(self, script_text, presubmit_path):
    """Executes a single presubmit script.

    Args:
      script_text: The text of the presubmit script.
      presubmit_path: The path to the presubmit file (this will be reported via
        input_api.PresubmitLocalPath()).

    Return:
      A list of result objects, empty if no problems.
    """
    input_api = InputApi(self.change, presubmit_path)
    context = {}
    exec script_text in context

    # These function names must change if we make substantial changes to
    # the presubmit API that are not backwards compatible.
    if self.committing:
      function_name = 'CheckChangeOnCommit'
    else:
      function_name = 'CheckChangeOnUpload'
    if function_name in context:
      context['__args'] = (input_api, OutputApi())
      result = eval(function_name + '(*__args)', context)
      if not (isinstance(result, types.TupleType) or
              isinstance(result, types.ListType)):
        raise exceptions.RuntimeError(
          'Presubmit functions must return a tuple or list')
      for item in result:
        if not isinstance(item, OutputApi.PresubmitResult):
          raise exceptions.RuntimeError(
            'All presubmit results must be of types derived from '
            'output_api.PresubmitResult')
    else:
      result = ()  # no error since the script doesn't care about current event.

    return result


def DoPresubmitChecks(change_info,
                      committing,
                      verbose,
                      output_stream,
                      input_stream,
                      default_presubmit):
  """Runs all presubmit checks that apply to the files in the change.

  This finds all PRESUBMIT.py files in directories enclosing the files in the
  change (up to the repository root) and calls the relevant entrypoint function
  depending on whether the change is being committed or uploaded.

  Prints errors, warnings and notifications.  Prompts the user for warnings
  when needed.

  Args:
    change_info: The ChangeInfo object for the change.
    committing: True if 'gcl commit' is running, False if 'gcl upload' is.
    verbose: Prints debug info.
    output_stream: A stream to write output from presubmit tests to.
    input_stream: A stream to read input from the user.
    default_presubmit: A default presubmit script to execute in any case.

  Return:
    True if execution can continue, False if not.
  """
  presubmit_files = ListRelevantPresubmitFiles(change_info.FileList())
  if not presubmit_files and verbose:
    output_stream.write("Warning, no presubmit.py found.")
  results = []
  executer = PresubmitExecuter(change_info, committing)
  if default_presubmit:
    if verbose:
      output_stream.write("Running default presubmit script")
    results += executer.ExecPresubmitScript(default_presubmit, 'PRESUBMIT.py')
  for filename in presubmit_files:
    filename = os.path.abspath(filename)
    if verbose:
      output_stream.write("Running %s" % filename)
    # Accept CRLF presubmit script.
    presubmit_script = gcl.ReadFile(filename, 'rU')
    results += executer.ExecPresubmitScript(presubmit_script, filename)

  errors = []
  notifications = []
  warnings = []
  for result in results:
    if not result.IsFatal() and not result.ShouldPrompt():
      notifications.append(result)
    elif result.ShouldPrompt():
      warnings.append(result)
    else:
      errors.append(result)

  error_count = 0
  for name, items in (('Messages', notifications),
                      ('Warnings', warnings),
                      ('ERRORS', errors)):
    if items:
      output_stream.write('\n** Presubmit %s **\n\n' % name)
      for item in items:
        if not item._Handle(output_stream, input_stream,
                            may_prompt=False):
          error_count += 1
        output_stream.write('\n')
  if not errors and warnings:
    output_stream.write(
      'There were presubmit warnings. Sure you want to continue? (y/N): ')
    response = input_stream.readline()
    if response.strip().lower() != 'y':
      error_count += 1
  return (error_count == 0)


def ScanSubDirs(mask, recursive):
  if not recursive:
    return [x for x in glob.glob(mask) if '.svn' not in x]
  else:
    results = []
    for root, dirs, files in os.walk('.'):
      if '.svn' in dirs:
        dirs.remove('.svn')
      for name in files:
        if fnmatch.fnmatch(name, mask):
          results.append(os.path.join(root, name))
    return results


def ParseFiles(args, recursive):
  files = []
  for arg in args:
    files.extend([('M', file) for file in ScanSubDirs(arg, recursive)])
  return files


def Main(argv):
  parser = optparse.OptionParser(usage="%prog [options]",
                                 version="%prog " + str(__version__))
  parser.add_option("-c", "--commit", action="store_true",
                   help="Use commit instead of upload checks")
  parser.add_option("-r", "--recursive", action="store_true",
                   help="Act recursively")
  parser.add_option("-v", "--verbose", action="store_true",
                   help="Verbose output")
  options, args = parser.parse_args(argv[1:])
  files = ParseFiles(args, options.recursive)
  if options.verbose:
    print "Found %d files." % len(files)
  return not DoPresubmitChecks(gcl.ChangeInfo(name='temp', files=files),
                               options.commit,
                               options.verbose,
                               sys.stdout,
                               sys.stdin,
                               default_presubmit=None)


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
