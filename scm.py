# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""SCM-specific utility classes."""

import os
import re
import subprocess
import sys
import tempfile
import xml.dom.minidom

import gclient_utils


class GIT(object):
  COMMAND = "git"

  @staticmethod
  def Capture(args, in_directory=None, print_error=True):
    """Runs git, capturing output sent to stdout as a string.

    Args:
      args: A sequence of command line parameters to be passed to git.
      in_directory: The directory where git is to be run.

    Returns:
      The output sent to stdout as a string.
    """
    c = [GIT.COMMAND]
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


  @staticmethod
  def CaptureStatus(files, upstream_branch='origin'):
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

    status = GIT.Capture(command).rstrip()
    results = []
    if status:
      for statusline in status.split('\n'):
        m = re.match('^(\w)\t(.+)$', statusline)
        if not m:
          raise Exception("status currently unsupported: %s" % statusline)
        results.append(('%s      ' % m.group(1), m.group(2)))
    return results

  @staticmethod
  def GetEmail(repo_root):
    """Retrieves the user email address if known."""
    # We could want to look at the svn cred when it has a svn remote but it
    # should be fine for now, users should simply configure their git settings.
    return GIT.Capture(['config', 'user.email'], repo_root).strip()


class SVN(object):
  COMMAND = "svn"

  @staticmethod
  def Run(args, in_directory):
    """Runs svn, sending output to stdout.

    Args:
      args: A sequence of command line parameters to be passed to svn.
      in_directory: The directory where svn is to be run.

    Raises:
      Error: An error occurred while running the svn command.
    """
    c = [SVN.COMMAND]
    c.extend(args)

    gclient_utils.SubprocessCall(c, in_directory)

  @staticmethod
  def Capture(args, in_directory=None, print_error=True):
    """Runs svn, capturing output sent to stdout as a string.

    Args:
      args: A sequence of command line parameters to be passed to svn.
      in_directory: The directory where svn is to be run.

    Returns:
      The output sent to stdout as a string.
    """
    c = [SVN.COMMAND]
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

  @staticmethod
  def RunAndGetFileList(options, args, in_directory, file_list):
    """Runs svn checkout, update, or status, output to stdout.

    The first item in args must be either "checkout", "update", or "status".

    svn's stdout is parsed to collect a list of files checked out or updated.
    These files are appended to file_list.  svn's stdout is also printed to
    sys.stdout as in Run.

    Args:
      options: command line options to gclient
      args: A sequence of command line parameters to be passed to svn.
      in_directory: The directory where svn is to be run.

    Raises:
      Error: An error occurred while running the svn command.
    """
    command = [SVN.COMMAND]
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
    # Place an upper limit.
    for i in range(1, 10):
      previous_list_len = len(file_list)
      failure = []
      def CaptureMatchingLines(line):
        match = compiled_pattern.search(line)
        if match:
          file_list.append(match.group(1))
        if line.startswith('svn: '):
          # We can't raise an exception. We can't alias a variable. Use a cheap
          # way.
          failure.append(True)
      try:
        SVN.RunAndFilterOutput(args,
                               in_directory,
                               options.verbose,
                               True,
                               CaptureMatchingLines)
      except gclient_utils.Error:
        # We enforce that some progress has been made.
        if len(failure) and len(file_list) > previous_list_len:
          if args[0] == 'checkout':
            args = args[:]
            # An aborted checkout is now an update.
            args[0] = 'update'
          continue
      break

  @staticmethod
  def RunAndFilterOutput(args,
                         in_directory,
                         print_messages,
                         print_stdout,
                         filter):
    """Runs svn checkout, update, status, or diff, optionally outputting
    to stdout.

    The first item in args must be either "checkout", "update",
    "status", or "diff".

    svn's stdout is passed line-by-line to the given filter function. If
    print_stdout is true, it is also printed to sys.stdout as in Run.

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
    command = [SVN.COMMAND]
    command.extend(args)

    gclient_utils.SubprocessCallAndFilter(command,
                                          in_directory,
                                          print_messages,
                                          print_stdout,
                                          filter=filter)

  @staticmethod
  def CaptureInfo(relpath, in_directory=None, print_error=True):
    """Returns a dictionary from the svn info output for the given file.

    Args:
      relpath: The directory where the working copy resides relative to
        the directory given by in_directory.
      in_directory: The directory where svn is to be run.
    """
    output = SVN.Capture(["info", "--xml", relpath], in_directory, print_error)
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
      result['Revision'] = C(GetNodeNamedAttributeText(dom, 'entry',
                                                       'revision'),
                             int)
      result['Node Kind'] = C(GetNodeNamedAttributeText(dom, 'entry', 'kind'),
                              str)
      # Differs across versions.
      if result['Node Kind'] == 'dir':
        result['Node Kind'] = 'directory'
      result['Schedule'] = C(GetNamedNodeText(dom, 'schedule'), str)
      result['Path'] = C(GetNodeNamedAttributeText(dom, 'entry', 'path'), str)
      result['Copied From URL'] = C(GetNamedNodeText(dom, 'copy-from-url'), str)
      result['Copied From Rev'] = C(GetNamedNodeText(dom, 'copy-from-rev'), str)
    return result

  @staticmethod
  def CaptureHeadRevision(url):
    """Get the head revision of a SVN repository.

    Returns:
      Int head revision
    """
    info = SVN.Capture(["info", "--xml", url], os.getcwd())
    dom = xml.dom.minidom.parseString(info)
    return dom.getElementsByTagName('entry')[0].getAttribute('revision')

  @staticmethod
  def CaptureStatus(files):
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
    dom = gclient_utils.ParseXML(SVN.Capture(command))
    results = []
    if dom:
      # /status/target/entry/(wc-status|commit|author|date)
      for target in dom.getElementsByTagName('target'):
        #base_path = target.getAttribute('path')
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

  @staticmethod
  def IsMoved(filename):
    """Determine if a file has been added through svn mv"""
    info = SVN.CaptureInfo(filename)
    return (info.get('Copied From URL') and
            info.get('Copied From Rev') and
            info.get('Schedule') == 'add')

  @staticmethod
  def GetFileProperty(file, property_name):
    """Returns the value of an SVN property for the given file.

    Args:
      file: The file to check
      property_name: The name of the SVN property, e.g. "svn:mime-type"

    Returns:
      The value of the property, which will be the empty string if the property
      is not set on the file.  If the file is not under version control, the
      empty string is also returned.
    """
    output = SVN.Capture(["propget", property_name, file])
    if (output.startswith("svn: ") and
        output.endswith("is not under version control")):
      return ""
    else:
      return output

  @staticmethod
  def DiffItem(filename):
    """Diff a single file"""
    # Use svn info output instead of os.path.isdir because the latter fails
    # when the file is deleted.
    if SVN.CaptureInfo(filename).get("Node Kind") == "directory":
      return None
    # If the user specified a custom diff command in their svn config file,
    # then it'll be used when we do svn diff, which we don't want to happen
    # since we want the unified diff.  Using --diff-cmd=diff doesn't always
    # work, since they can have another diff executable in their path that
    # gives different line endings.  So we use a bogus temp directory as the
    # config directory, which gets around these problems.
    if sys.platform.startswith("win"):
      parent_dir = tempfile.gettempdir()
    else:
      parent_dir = sys.path[0]  # tempdir is not secure.
    bogus_dir = os.path.join(parent_dir, "temp_svn_config")
    if not os.path.exists(bogus_dir):
      os.mkdir(bogus_dir)
    # Grabs the diff data.
    data = SVN.Capture(["diff", "--config-dir", bogus_dir, filename], None)

    # We know the diff will be incorrectly formatted. Fix it.
    if SVN.IsMoved(filename):
      # The file is "new" in the patch sense. Generate a homebrew diff.
      # We can't use ReadFile() since it's not using binary mode.
      file_handle = open(filename, 'rb')
      file_content = file_handle.read()
      file_handle.close()
      # Prepend '+' to every lines.
      file_content = ['+' + i for i in file_content.splitlines(True)]
      nb_lines = len(file_content)
      # We need to use / since patch on unix will fail otherwise.
      filename = filename.replace('\\', '/')
      data = "Index: %s\n" % filename
      data += ("============================================================="
               "======\n")
      # Note: Should we use /dev/null instead?
      data += "--- %s\n" % filename
      data += "+++ %s\n" % filename
      data += "@@ -0,0 +1,%d @@\n" % nb_lines
      data += ''.join(file_content)
    return data

  @staticmethod
  def GetEmail(repo_root):
    """Retrieves the svn account which we assume is an email address."""
    infos = SVN.CaptureInfo(repo_root)
    uuid = infos.get('UUID')
    root = infos.get('Repository Root')
    if not root:
      return None

    # Should check for uuid but it is incorrectly saved for https creds.
    realm = root.rsplit('/', 1)[0]
    if root.startswith('https') or not uuid:
      regexp = re.compile(r'<%s:\d+>.*' % realm)
    else:
      regexp = re.compile(r'<%s:\d+> %s' % (realm, uuid))
    if regexp is None:
      return None
    if sys.platform.startswith('win'):
      if not 'APPDATA' in os.environ:
        return None
      auth_dir = os.path.join(os.environ['APPDATA'], 'Subversion', 'auth',
                              'svn.simple')
    else:
      if not 'HOME' in os.environ:
        return None
      auth_dir = os.path.join(os.environ['HOME'], '.subversion', 'auth',
                              'svn.simple')
    for credfile in os.listdir(auth_dir):
      cred_info = SVN.ReadSimpleAuth(os.path.join(auth_dir, credfile))
      if regexp.match(cred_info.get('svn:realmstring')):
        return cred_info.get('username')

  @staticmethod
  def ReadSimpleAuth(filename):
    f = open(filename, 'r')
    values = {}
    def ReadOneItem(type):
      m = re.match(r'%s (\d+)' % type, f.readline())
      if not m:
        return None
      data = f.read(int(m.group(1)))
      if f.read(1) != '\n':
        return None
      return data

    while True:
      key = ReadOneItem('K')
      if not key:
        break
      value = ReadOneItem('V')
      if not value:
        break
      values[key] = value
    return values
