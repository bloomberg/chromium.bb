# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""SCM-specific functions."""

import os
import re
import subprocess
import sys
import xml.dom.minidom

import gclient_utils


SVN_COMMAND = "svn"
GIT_COMMAND = "git"

# -----------------------------------------------------------------------------
# Git utils:


def CaptureGit(args, in_directory=None, print_error=True):
  """Runs git, capturing output sent to stdout as a string.

  Args:
    args: A sequence of command line parameters to be passed to git.
    in_directory: The directory where git is to be run.

  Returns:
    The output sent to stdout as a string.
  """
  c = [GIT_COMMAND]
  c.extend(args)

  # *Sigh*:  Windows needs shell=True, or else it won't search %PATH% for
  # the git.exe executable, but shell=True makes subprocess on Linux fail
  # when it's called with a list because it only tries to execute the
  # first string ("git").
  stderr = None
  if not print_error:
    stderr = subprocess.PIPE
  return subprocess.Popen(c,
                          cwd=in_directory,
                          shell=sys.platform.startswith('win'),
                          stdout=subprocess.PIPE,
                          stderr=stderr).communicate()[0]


def CaptureGitStatus(files, upstream_branch='origin'):
  """Returns git status.

  @files can be a string (one file) or a list of files.

  Returns an array of (status, file) tuples."""
  command = ["diff", "--name-status", "-r", "%s.." % upstream_branch]
  if not files:
    pass
  elif isinstance(files, basestring):
    command.append(files)
  else:
    command.extend(files)

  status = CaptureGit(command).rstrip()
  results = []
  if status:
    for statusline in status.split('\n'):
      m = re.match('^(\w)\t(.+)$', statusline)
      if not m:
        raise Exception("status currently unsupported: %s" % statusline)
      results.append(('%s      ' % m.group(1), m.group(2)))
  return results


# -----------------------------------------------------------------------------
# SVN utils:


def RunSVN(args, in_directory):
  """Runs svn, sending output to stdout.

  Args:
    args: A sequence of command line parameters to be passed to svn.
    in_directory: The directory where svn is to be run.

  Raises:
    Error: An error occurred while running the svn command.
  """
  c = [SVN_COMMAND]
  c.extend(args)

  gclient_utils.SubprocessCall(c, in_directory)


def CaptureSVN(args, in_directory=None, print_error=True):
  """Runs svn, capturing output sent to stdout as a string.

  Args:
    args: A sequence of command line parameters to be passed to svn.
    in_directory: The directory where svn is to be run.

  Returns:
    The output sent to stdout as a string.
  """
  c = [SVN_COMMAND]
  c.extend(args)

  # *Sigh*:  Windows needs shell=True, or else it won't search %PATH% for
  # the svn.exe executable, but shell=True makes subprocess on Linux fail
  # when it's called with a list because it only tries to execute the
  # first string ("svn").
  stderr = None
  if not print_error:
    stderr = subprocess.PIPE
  return subprocess.Popen(c,
                          cwd=in_directory,
                          shell=(sys.platform == 'win32'),
                          stdout=subprocess.PIPE,
                          stderr=stderr).communicate()[0]


def RunSVNAndGetFileList(options, args, in_directory, file_list):
  """Runs svn checkout, update, or status, output to stdout.

  The first item in args must be either "checkout", "update", or "status".

  svn's stdout is parsed to collect a list of files checked out or updated.
  These files are appended to file_list.  svn's stdout is also printed to
  sys.stdout as in RunSVN.

  Args:
    options: command line options to gclient
    args: A sequence of command line parameters to be passed to svn.
    in_directory: The directory where svn is to be run.

  Raises:
    Error: An error occurred while running the svn command.
  """
  command = [SVN_COMMAND]
  command.extend(args)

  # svn update and svn checkout use the same pattern: the first three columns
  # are for file status, property status, and lock status.  This is followed
  # by two spaces, and then the path to the file.
  update_pattern = '^...  (.*)$'

  # The first three columns of svn status are the same as for svn update and
  # svn checkout.  The next three columns indicate addition-with-history,
  # switch, and remote lock status.  This is followed by one space, and then
  # the path to the file.
  status_pattern = '^...... (.*)$'

  # args[0] must be a supported command.  This will blow up if it's something
  # else, which is good.  Note that the patterns are only effective when
  # these commands are used in their ordinary forms, the patterns are invalid
  # for "svn status --show-updates", for example.
  pattern = {
        'checkout': update_pattern,
        'status':   status_pattern,
        'update':   update_pattern,
      }[args[0]]

  compiled_pattern = re.compile(pattern)

  def CaptureMatchingLines(line):
    match = compiled_pattern.search(line)
    if match:
      file_list.append(match.group(1))

  RunSVNAndFilterOutput(args,
                        in_directory,
                        options.verbose,
                        True,
                        CaptureMatchingLines)

def RunSVNAndFilterOutput(args,
                          in_directory,
                          print_messages,
                          print_stdout,
                          filter):
  """Runs svn checkout, update, status, or diff, optionally outputting
  to stdout.

  The first item in args must be either "checkout", "update",
  "status", or "diff".

  svn's stdout is passed line-by-line to the given filter function. If
  print_stdout is true, it is also printed to sys.stdout as in RunSVN.

  Args:
    args: A sequence of command line parameters to be passed to svn.
    in_directory: The directory where svn is to be run.
    print_messages: Whether to print status messages to stdout about
      which Subversion commands are being run.
    print_stdout: Whether to forward Subversion's output to stdout.
    filter: A function taking one argument (a string) which will be
      passed each line (with the ending newline character removed) of
      Subversion's output for filtering.

  Raises:
    Error: An error occurred while running the svn command.
  """
  command = [SVN_COMMAND]
  command.extend(args)

  gclient_utils.SubprocessCallAndFilter(command,
                                        in_directory,
                                        print_messages,
                                        print_stdout,
                                        filter=filter)

def CaptureSVNInfo(relpath, in_directory=None, print_error=True):
  """Returns a dictionary from the svn info output for the given file.

  Args:
    relpath: The directory where the working copy resides relative to
      the directory given by in_directory.
    in_directory: The directory where svn is to be run.
  """
  output = CaptureSVN(["info", "--xml", relpath], in_directory, print_error)
  dom = gclient_utils.ParseXML(output)
  result = {}
  if dom:
    GetNamedNodeText = gclient_utils.GetNamedNodeText
    GetNodeNamedAttributeText = gclient_utils.GetNodeNamedAttributeText
    def C(item, f):
      if item is not None: return f(item)
    # /info/entry/
    #   url
    #   reposityory/(root|uuid)
    #   wc-info/(schedule|depth)
    #   commit/(author|date)
    # str() the results because they may be returned as Unicode, which
    # interferes with the higher layers matching up things in the deps
    # dictionary.
    # TODO(maruel): Fix at higher level instead (!)
    result['Repository Root'] = C(GetNamedNodeText(dom, 'root'), str)
    result['URL'] = C(GetNamedNodeText(dom, 'url'), str)
    result['UUID'] = C(GetNamedNodeText(dom, 'uuid'), str)
    result['Revision'] = C(GetNodeNamedAttributeText(dom, 'entry', 'revision'),
                           int)
    result['Node Kind'] = C(GetNodeNamedAttributeText(dom, 'entry', 'kind'),
                            str)
    result['Schedule'] = C(GetNamedNodeText(dom, 'schedule'), str)
    result['Path'] = C(GetNodeNamedAttributeText(dom, 'entry', 'path'), str)
    result['Copied From URL'] = C(GetNamedNodeText(dom, 'copy-from-url'), str)
    result['Copied From Rev'] = C(GetNamedNodeText(dom, 'copy-from-rev'), str)
  return result


def CaptureSVNHeadRevision(url):
  """Get the head revision of a SVN repository.

  Returns:
    Int head revision
  """
  info = CaptureSVN(["info", "--xml", url], os.getcwd())
  dom = xml.dom.minidom.parseString(info)
  return dom.getElementsByTagName('entry')[0].getAttribute('revision')


def CaptureSVNStatus(files):
  """Returns the svn 1.5 svn status emulated output.

  @files can be a string (one file) or a list of files.

  Returns an array of (status, file) tuples."""
  command = ["status", "--xml"]
  if not files:
    pass
  elif isinstance(files, basestring):
    command.append(files)
  else:
    command.extend(files)

  status_letter = {
    None: ' ',
    '': ' ',
    'added': 'A',
    'conflicted': 'C',
    'deleted': 'D',
    'external': 'X',
    'ignored': 'I',
    'incomplete': '!',
    'merged': 'G',
    'missing': '!',
    'modified': 'M',
    'none': ' ',
    'normal': ' ',
    'obstructed': '~',
    'replaced': 'R',
    'unversioned': '?',
  }
  dom = gclient_utils.ParseXML(CaptureSVN(command))
  results = []
  if dom:
    # /status/target/entry/(wc-status|commit|author|date)
    for target in dom.getElementsByTagName('target'):
      for entry in target.getElementsByTagName('entry'):
        file_path = entry.getAttribute('path')
        wc_status = entry.getElementsByTagName('wc-status')
        assert len(wc_status) == 1
        # Emulate svn 1.5 status ouput...
        statuses = [' '] * 7
        # Col 0
        xml_item_status = wc_status[0].getAttribute('item')
        if xml_item_status in status_letter:
          statuses[0] = status_letter[xml_item_status]
        else:
          raise Exception('Unknown item status "%s"; please implement me!' %
                          xml_item_status)
        # Col 1
        xml_props_status = wc_status[0].getAttribute('props')
        if xml_props_status == 'modified':
          statuses[1] = 'M'
        elif xml_props_status == 'conflicted':
          statuses[1] = 'C'
        elif (not xml_props_status or xml_props_status == 'none' or
              xml_props_status == 'normal'):
          pass
        else:
          raise Exception('Unknown props status "%s"; please implement me!' %
                          xml_props_status)
        # Col 2
        if wc_status[0].getAttribute('wc-locked') == 'true':
          statuses[2] = 'L'
        # Col 3
        if wc_status[0].getAttribute('copied') == 'true':
          statuses[3] = '+'
        # Col 4
        if wc_status[0].getAttribute('switched') == 'true':
          statuses[4] = 'S'
        # TODO(maruel): Col 5 and 6
        item = (''.join(statuses), file_path)
        results.append(item)
  return results
