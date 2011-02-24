# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""SCM-specific utility classes."""

import cStringIO
import glob
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import xml.dom.minidom

import gclient_utils

def ValidateEmail(email):
  return (re.match(r"^[a-zA-Z0-9._%-+]+@[a-zA-Z0-9._%-]+.[a-zA-Z]{2,6}$", email)
          is not None)


def GetCasedPath(path):
  """Elcheapos way to get the real path case on Windows."""
  if sys.platform.startswith('win') and os.path.exists(path):
    # Reconstruct the path.
    path = os.path.abspath(path)
    paths = path.split('\\')
    for i in range(len(paths)):
      if i == 0:
        # Skip drive letter.
        continue
      subpath = '\\'.join(paths[:i+1])
      prev = len('\\'.join(paths[:i]))
      # glob.glob will return the cased path for the last item only. This is why
      # we are calling it in a loop. Extract the data we want and put it back
      # into the list.
      paths[i] = glob.glob(subpath + '*')[0][prev+1:len(subpath)]
    path = '\\'.join(paths)
  return path


def GenFakeDiff(filename):
  """Generates a fake diff from a file."""
  file_content = gclient_utils.FileRead(filename, 'rb').splitlines(True)
  filename = filename.replace(os.sep, '/')
  nb_lines = len(file_content)
  # We need to use / since patch on unix will fail otherwise.
  data = cStringIO.StringIO()
  data.write("Index: %s\n" % filename)
  data.write('=' * 67 + '\n')
  # Note: Should we use /dev/null instead?
  data.write("--- %s\n" % filename)
  data.write("+++ %s\n" % filename)
  data.write("@@ -0,0 +1,%d @@\n" % nb_lines)
  # Prepend '+' to every lines.
  for line in file_content:
    data.write('+')
    data.write(line)
  result = data.getvalue()
  data.close()
  return result


class GIT(object):
  @staticmethod
  def Capture(args, **kwargs):
    return gclient_utils.CheckCall(['git'] + args, print_error=False,
        **kwargs)[0]

  @staticmethod
  def CaptureStatus(files, upstream_branch=None):
    """Returns git status.

    @files can be a string (one file) or a list of files.

    Returns an array of (status, file) tuples."""
    if upstream_branch is None:
      upstream_branch = GIT.GetUpstreamBranch(os.getcwd())
      if upstream_branch is None:
        raise gclient_utils.Error('Cannot determine upstream branch')
    command = ['diff', '--name-status', '-r', '%s...' % upstream_branch]
    if not files:
      pass
    elif isinstance(files, basestring):
      command.append(files)
    else:
      command.extend(files)
    status = GIT.Capture(command).rstrip()
    results = []
    if status:
      for statusline in status.splitlines():
        # 3-way merges can cause the status can be 'MMM' instead of 'M'. This
        # can happen when the user has 2 local branches and he diffs between
        # these 2 branches instead diffing to upstream.
        m = re.match('^(\w)+\t(.+)$', statusline)
        if not m:
          raise gclient_utils.Error(
              'status currently unsupported: %s' % statusline)
        # Only grab the first letter.
        results.append(('%s      ' % m.group(1)[0], m.group(2)))
    return results

  @staticmethod
  def GetEmail(cwd):
    """Retrieves the user email address if known."""
    # We could want to look at the svn cred when it has a svn remote but it
    # should be fine for now, users should simply configure their git settings.
    try:
      return GIT.Capture(['config', 'user.email'], cwd=cwd).strip()
    except gclient_utils.CheckCallError:
      return ''

  @staticmethod
  def ShortBranchName(branch):
    """Converts a name like 'refs/heads/foo' to just 'foo'."""
    return branch.replace('refs/heads/', '')

  @staticmethod
  def GetBranchRef(cwd):
    """Returns the full branch reference, e.g. 'refs/heads/master'."""
    return GIT.Capture(['symbolic-ref', 'HEAD'], cwd=cwd).strip()

  @staticmethod
  def GetBranch(cwd):
    """Returns the short branch name, e.g. 'master'."""
    return GIT.ShortBranchName(GIT.GetBranchRef(cwd))

  @staticmethod
  def IsGitSvn(cwd):
    """Returns true if this repo looks like it's using git-svn."""
    # If you have any "svn-remote.*" config keys, we think you're using svn.
    try:
      GIT.Capture(['config', '--get-regexp', r'^svn-remote\.'], cwd=cwd)
      return True
    except gclient_utils.CheckCallError:
      return False

  @staticmethod
  def GetSVNBranch(cwd):
    """Returns the svn branch name if found."""
    # Try to figure out which remote branch we're based on.
    # Strategy:
    # 1) find all git-svn branches and note their svn URLs.
    # 2) iterate through our branch history and match up the URLs.

    # regexp matching the git-svn line that contains the URL.
    git_svn_re = re.compile(r'^\s*git-svn-id: (\S+)@', re.MULTILINE)

    # Get the refname and svn url for all refs/remotes/*.
    remotes = GIT.Capture(
        ['for-each-ref', '--format=%(refname)', 'refs/remotes'],
        cwd=cwd).splitlines()
    svn_refs = {}
    for ref in remotes:
      match = git_svn_re.search(
          GIT.Capture(['cat-file', '-p', ref], cwd=cwd))
      # Prefer origin/HEAD over all others.
      if match and (match.group(1) not in svn_refs or
                    ref == "refs/remotes/origin/HEAD"):
        svn_refs[match.group(1)] = ref

    svn_branch = ''
    if len(svn_refs) == 1:
      # Only one svn branch exists -- seems like a good candidate.
      svn_branch = svn_refs.values()[0]
    elif len(svn_refs) > 1:
      # We have more than one remote branch available.  We don't
      # want to go through all of history, so read a line from the
      # pipe at a time.
      # The -100 is an arbitrary limit so we don't search forever.
      cmd = ['git', 'log', '-100', '--pretty=medium']
      proc = gclient_utils.Popen(cmd, stdout=subprocess.PIPE, cwd=cwd)
      for line in proc.stdout:
        match = git_svn_re.match(line)
        if match:
          url = match.group(1)
          if url in svn_refs:
            svn_branch = svn_refs[url]
            proc.stdout.close()  # Cut pipe.
            break
    return svn_branch

  @staticmethod
  def FetchUpstreamTuple(cwd):
    """Returns a tuple containg remote and remote ref,
       e.g. 'origin', 'refs/heads/master'
       Tries to be intelligent and understand git-svn.
    """
    remote = '.'
    branch = GIT.GetBranch(cwd)
    try:
      upstream_branch = GIT.Capture(
          ['config', 'branch.%s.merge' % branch], cwd=cwd).strip()
    except gclient_utils.Error:
      upstream_branch = None
    if upstream_branch:
      try:
        remote = GIT.Capture(
            ['config', 'branch.%s.remote' % branch], cwd=cwd).strip()
      except gclient_utils.Error:
        pass
    else:
      # Fall back on trying a git-svn upstream branch.
      if GIT.IsGitSvn(cwd):
        upstream_branch = GIT.GetSVNBranch(cwd)
      else:
        # Else, try to guess the origin remote.
        remote_branches = GIT.Capture(['branch', '-r'], cwd=cwd).split()
        if 'origin/master' in remote_branches:
          # Fall back on origin/master if it exits.
          remote = 'origin'
          upstream_branch = 'refs/heads/master'
        elif 'origin/trunk' in remote_branches:
          # Fall back on origin/trunk if it exists. Generally a shared
          # git-svn clone
          remote = 'origin'
          upstream_branch = 'refs/heads/trunk'
        else:
          # Give up.
          remote = None
          upstream_branch = None
    return remote, upstream_branch

  @staticmethod
  def GetUpstreamBranch(cwd):
    """Gets the current branch's upstream branch."""
    remote, upstream_branch = GIT.FetchUpstreamTuple(cwd)
    if remote != '.' and upstream_branch:
      upstream_branch = upstream_branch.replace('heads', 'remotes/' + remote)
    return upstream_branch

  @staticmethod
  def GenerateDiff(cwd, branch=None, branch_head='HEAD', full_move=False,
                   files=None):
    """Diffs against the upstream branch or optionally another branch.

    full_move means that move or copy operations should completely recreate the
    files, usually in the prospect to apply the patch for a try job."""
    if not branch:
      branch = GIT.GetUpstreamBranch(cwd)
    command = ['diff', '-p', '--no-prefix', '--no-ext-diff',
               branch + "..." + branch_head]
    if not full_move:
      command.append('-C')
    # TODO(maruel): --binary support.
    if files:
      command.append('--')
      command.extend(files)
    diff = GIT.Capture(command, cwd=cwd).splitlines(True)
    for i in range(len(diff)):
      # In the case of added files, replace /dev/null with the path to the
      # file being added.
      if diff[i].startswith('--- /dev/null'):
        diff[i] = '--- %s' % diff[i+1][4:]
    return ''.join(diff)

  @staticmethod
  def GetDifferentFiles(cwd, branch=None, branch_head='HEAD'):
    """Returns the list of modified files between two branches."""
    if not branch:
      branch = GIT.GetUpstreamBranch(cwd)
    command = ['diff', '--name-only', branch + "..." + branch_head]
    return GIT.Capture(command, cwd=cwd).splitlines(False)

  @staticmethod
  def GetPatchName(cwd):
    """Constructs a name for this patch."""
    short_sha = GIT.Capture(['rev-parse', '--short=4', 'HEAD'], cwd=cwd).strip()
    return "%s#%s" % (GIT.GetBranch(cwd), short_sha)

  @staticmethod
  def GetCheckoutRoot(cwd):
    """Returns the top level directory of a git checkout as an absolute path.
    """
    root = GIT.Capture(['rev-parse', '--show-cdup'], cwd=cwd).strip()
    return os.path.abspath(os.path.join(cwd, root))

  @staticmethod
  def AssertVersion(min_version):
    """Asserts git's version is at least min_version."""
    def only_int(val):
      if val.isdigit():
        return int(val)
      else:
        return 0
    current_version =  GIT.Capture(['--version']).split()[-1]
    current_version_list = map(only_int, current_version.split('.'))
    for min_ver in map(int, min_version.split('.')):
      ver = current_version_list.pop(0)
      if ver < min_ver:
        return (False, current_version)
      elif ver > min_ver:
        return (True, current_version)
    return (True, current_version)


class SVN(object):
  current_version = None

  @staticmethod
  def Capture(args, **kwargs):
    """Always redirect stderr.

    Throws an exception if non-0 is returned."""
    return gclient_utils.CheckCall(['svn'] + args, print_error=False,
        **kwargs)[0]

  @staticmethod
  def RunAndGetFileList(verbose, args, cwd, file_list, stdout=None):
    """Runs svn checkout, update, or status, output to stdout.

    The first item in args must be either "checkout", "update", or "status".

    svn's stdout is parsed to collect a list of files checked out or updated.
    These files are appended to file_list.  svn's stdout is also printed to
    sys.stdout as in Run.

    Args:
      verbose: If True, uses verbose output
      args: A sequence of command line parameters to be passed to svn.
      cwd: The directory where svn is to be run.

    Raises:
      Error: An error occurred while running the svn command.
    """
    stdout = stdout or sys.stdout

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
    backoff_time = 5
    retries = 0
    while True:
      retries += 1
      previous_list_len = len(file_list)
      failure = []

      def CaptureMatchingLines(line):
        match = compiled_pattern.search(line)
        if match:
          file_list.append(match.group(1))
        if line.startswith('svn: '):
          failure.append(line)

      try:
        gclient_utils.CheckCallAndFilterAndHeader(
            ['svn'] + args,
            cwd=cwd,
            always=verbose,
            filter_fn=CaptureMatchingLines,
            stdout=stdout)
      except gclient_utils.Error:
        def IsKnownFailure():
          for x in failure:
            if (x.startswith('svn: OPTIONS of') or
                x.startswith('svn: PROPFIND of') or
                x.startswith('svn: REPORT of') or
                x.startswith('svn: Unknown hostname') or
                x.startswith('svn: Server sent unexpected return value')):
              return True
          return False

        # Subversion client is really misbehaving with Google Code.
        if args[0] == 'checkout':
          # Ensure at least one file was checked out, otherwise *delete* the
          # directory.
          if len(file_list) == previous_list_len:
            if not IsKnownFailure():
              # No known svn error was found, bail out.
              raise
            # No file were checked out, so make sure the directory is
            # deleted in case it's messed up and try again.
            # Warning: It's bad, it assumes args[2] is the directory
            # argument.
            if os.path.isdir(args[2]):
              gclient_utils.RemoveDirectory(args[2])
          else:
            # Progress was made, convert to update since an aborted checkout
            # is now an update.
            args = ['update'] + args[1:]
        else:
          # It was an update or export.
          # We enforce that some progress has been made or a known failure.
          if len(file_list) == previous_list_len and not IsKnownFailure():
            # No known svn error was found and no progress, bail out.
            raise
        if retries == 10:
          raise
        print "Sleeping %.1f seconds and retrying...." % backoff_time
        time.sleep(backoff_time)
        backoff_time *= 1.3
        continue
      break

  @staticmethod
  def CaptureInfo(cwd):
    """Returns a dictionary from the svn info output for the given file.

    Throws an exception if svn info fails."""
    output = SVN.Capture(['info', '--xml', cwd])
    dom = gclient_utils.ParseXML(output)
    result = {}
    if dom:
      GetNamedNodeText = gclient_utils.GetNamedNodeText
      GetNodeNamedAttributeText = gclient_utils.GetNodeNamedAttributeText
      def C(item, f):
        if item is not None:
          return f(item)
      # /info/entry/
      #   url
      #   reposityory/(root|uuid)
      #   wc-info/(schedule|depth)
      #   commit/(author|date)
      # str() the results because they may be returned as Unicode, which
      # interferes with the higher layers matching up things in the deps
      # dictionary.
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
  def CaptureRevision(cwd):
    """Get the base revision of a SVN repository.

    Returns:
      Int base revision
    """
    info = SVN.Capture(['info', '--xml'], cwd=cwd)
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
            raise gclient_utils.Error(
                'Unknown item status "%s"; please implement me!' %
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
            raise gclient_utils.Error(
                'Unknown props status "%s"; please implement me!' %
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
    return SVN.IsMovedInfo(SVN.CaptureInfo(filename))

  @staticmethod
  def IsMovedInfo(info):
    """Determine if a file has been added through svn mv"""
    return (info.get('Copied From URL') and
            info.get('Copied From Rev') and
            info.get('Schedule') == 'add')

  @staticmethod
  def GetFileProperty(filename, property_name):
    """Returns the value of an SVN property for the given file.

    Args:
      filename: The file to check
      property_name: The name of the SVN property, e.g. "svn:mime-type"

    Returns:
      The value of the property, which will be the empty string if the property
      is not set on the file.  If the file is not under version control, the
      empty string is also returned.
    """
    try:
      return SVN.Capture(['propget', property_name, filename])
    except gclient_utils.Error:
      return ''

  @staticmethod
  def DiffItem(filename, full_move=False, revision=None):
    """Diffs a single file.

    Should be simple, eh? No it isn't.
    Be sure to be in the appropriate directory before calling to have the
    expected relative path.
    full_move means that move or copy operations should completely recreate the
    files, usually in the prospect to apply the patch for a try job."""
    # If the user specified a custom diff command in their svn config file,
    # then it'll be used when we do svn diff, which we don't want to happen
    # since we want the unified diff.  Using --diff-cmd=diff doesn't always
    # work, since they can have another diff executable in their path that
    # gives different line endings.  So we use a bogus temp directory as the
    # config directory, which gets around these problems.
    bogus_dir = tempfile.mkdtemp()
    try:
      # Use "svn info" output instead of os.path.isdir because the latter fails
      # when the file is deleted.
      return SVN._DiffItemInternal(filename, SVN.CaptureInfo(filename),
                                   bogus_dir,
                                   full_move=full_move, revision=revision)
    finally:
      shutil.rmtree(bogus_dir)

  @staticmethod
  def _DiffItemInternal(filename, info, bogus_dir, full_move=False,
                        revision=None):
    """Grabs the diff data."""
    command = ["diff", "--config-dir", bogus_dir, filename]
    if revision:
      command.extend(['--revision', revision])
    data = None
    if SVN.IsMovedInfo(info):
      if full_move:
        if info.get("Node Kind") == "directory":
          # Things become tricky here. It's a directory copy/move. We need to
          # diff all the files inside it.
          # This will put a lot of pressure on the heap. This is why StringIO
          # is used and converted back into a string at the end. The reason to
          # return a string instead of a StringIO is that StringIO.write()
          # doesn't accept a StringIO object. *sigh*.
          for (dirpath, dirnames, filenames) in os.walk(filename):
            # Cleanup all files starting with a '.'.
            for d in dirnames:
              if d.startswith('.'):
                dirnames.remove(d)
            for f in filenames:
              if f.startswith('.'):
                filenames.remove(f)
            for f in filenames:
              if data is None:
                data = cStringIO.StringIO()
              data.write(GenFakeDiff(os.path.join(dirpath, f)))
          if data:
            tmp = data.getvalue()
            data.close()
            data = tmp
        else:
          data = GenFakeDiff(filename)
      else:
        if info.get("Node Kind") != "directory":
          # svn diff on a mv/cp'd file outputs nothing if there was no change.
          data = SVN.Capture(command)
          if not data:
            # We put in an empty Index entry so upload.py knows about them.
            data = "Index: %s\n" % filename.replace(os.sep, '/')
        # Otherwise silently ignore directories.
    else:
      if info.get("Node Kind") != "directory":
        # Normal simple case.
        data = SVN.Capture(command)
      # Otherwise silently ignore directories.
    return data

  @staticmethod
  def GenerateDiff(filenames, root=None, full_move=False, revision=None):
    """Returns a string containing the diff for the given file list.

    The files in the list should either be absolute paths or relative to the
    given root. If no root directory is provided, the repository root will be
    used.
    The diff will always use relative paths.
    """
    assert isinstance(filenames, (list, tuple))
    previous_cwd = os.getcwd()
    root = root or SVN.GetCheckoutRoot(previous_cwd)
    root = os.path.normcase(os.path.join(root, ''))
    def RelativePath(path, root):
      """We must use relative paths."""
      if os.path.normcase(path).startswith(root):
        return path[len(root):]
      return path
    # If the user specified a custom diff command in their svn config file,
    # then it'll be used when we do svn diff, which we don't want to happen
    # since we want the unified diff.  Using --diff-cmd=diff doesn't always
    # work, since they can have another diff executable in their path that
    # gives different line endings.  So we use a bogus temp directory as the
    # config directory, which gets around these problems.
    bogus_dir = tempfile.mkdtemp()
    try:
      os.chdir(root)
      # Cleanup filenames
      filenames = [RelativePath(f, root) for f in filenames]
      # Get information about the modified items (files and directories)
      data = dict([(f, SVN.CaptureInfo(f)) for f in filenames])
      diffs = []
      if full_move:
        # Eliminate modified files inside moved/copied directory.
        for (filename, info) in data.iteritems():
          if SVN.IsMovedInfo(info) and info.get("Node Kind") == "directory":
            # Remove files inside the directory.
            filenames = [f for f in filenames
                         if not f.startswith(filename + os.path.sep)]
        for filename in data.keys():
          if not filename in filenames:
            # Remove filtered out items.
            del data[filename]
      else:
        metaheaders = []
        for (filename, info) in data.iteritems():
          if SVN.IsMovedInfo(info):
            # for now, the most common case is a head copy,
            # so let's just encode that as a straight up cp.
            srcurl = info.get('Copied From URL')
            root = info.get('Repository Root')
            rev = int(info.get('Copied From Rev'))
            assert srcurl.startswith(root)
            src = srcurl[len(root)+1:]
            try:
              srcinfo = SVN.CaptureInfo(srcurl)
            except gclient_utils.CheckCallError, e:
              if not 'Not a valid URL' in e.stderr:
                raise
              # Assume the file was deleted. No idea how to figure out at which
              # revision the file was deleted.
              srcinfo = {'Revision': rev}
            if (srcinfo.get('Revision') != rev and
                SVN.Capture(['diff', '-r', '%d:head' % rev, srcurl])):
              metaheaders.append("#$ svn cp -r %d %s %s "
                                 "### WARNING: note non-trunk copy\n" %
                                 (rev, src, filename))
            else:
              metaheaders.append("#$ cp %s %s\n" % (src,
                                                    filename))

        if metaheaders:
          diffs.append("### BEGIN SVN COPY METADATA\n")
          diffs.extend(metaheaders)
          diffs.append("### END SVN COPY METADATA\n")
      # Now ready to do the actual diff.
      for filename in sorted(data.iterkeys()):
        diffs.append(SVN._DiffItemInternal(filename, data[filename], bogus_dir,
                                           full_move=full_move,
                                           revision=revision))
      # Use StringIO since it can be messy when diffing a directory move with
      # full_move=True.
      buf = cStringIO.StringIO()
      for d in filter(None, diffs):
        buf.write(d)
      result = buf.getvalue()
      buf.close()
      return result
    finally:
      os.chdir(previous_cwd)
      shutil.rmtree(bogus_dir)

  @staticmethod
  def GetEmail(repo_root):
    """Retrieves the svn account which we assume is an email address."""
    try:
      infos = SVN.CaptureInfo(repo_root)
    except gclient_utils.Error:
      return None

    # Should check for uuid but it is incorrectly saved for https creds.
    root = infos['Repository Root']
    realm = root.rsplit('/', 1)[0]
    uuid = infos['UUID']
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
    def ReadOneItem(item_type):
      m = re.match(r'%s (\d+)' % item_type, f.readline())
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

  @staticmethod
  def GetCheckoutRoot(directory):
    """Returns the top level directory of the current repository.

    The directory is returned as an absolute path.
    """
    directory = os.path.abspath(directory)
    try:
      cur_dir_repo_root = SVN.CaptureInfo(directory)['Repository Root']
    except gclient_utils.Error:
      return None
    while True:
      parent = os.path.dirname(directory)
      try:
        if SVN.CaptureInfo(parent)['Repository Root'] != cur_dir_repo_root:
          break
      except gclient_utils.Error:
        break
      directory = parent
    return GetCasedPath(directory)

  @staticmethod
  def AssertVersion(min_version):
    """Asserts svn's version is at least min_version."""
    def only_int(val):
      if val.isdigit():
        return int(val)
      else:
        return 0
    if not SVN.current_version:
      SVN.current_version = SVN.Capture(['--version']).split()[2]
    current_version_list = map(only_int, SVN.current_version.split('.'))
    for min_ver in map(int, min_version.split('.')):
      ver = current_version_list.pop(0)
      if ver < min_ver:
        return (False, SVN.current_version)
      elif ver > min_ver:
        return (True, SVN.current_version)
    return (True, SVN.current_version)

  @staticmethod
  def Revert(repo_root, callback=None, ignore_externals=False):
    """Reverts all svn modifications in repo_root, including properties.

    Deletes any modified files or directory.

    A "svn update --revision BASE" call is required after to revive deleted
    files.
    """
    for file_status in SVN.CaptureStatus(repo_root):
      file_path = os.path.join(repo_root, file_status[1])
      if ignore_externals and file_status[0][0] == 'X':
        # Ignore externals.
        logging.info('Ignoring external %s' % file_status[1])
        continue

      if callback:
        callback(file_status)

      if file_status[0].isspace():
        # Try reverting the file since it's probably a property change.
        gclient_utils.CheckCall(
            ['svn', 'revert', file_status[1]], cwd=repo_root)

      # svn revert is really stupid. It fails on inconsistent line-endings,
      # on switched directories, etc. So take no chance and delete everything!
      if file_status[0][0] == 'D':
        # Deleted file requires manual intervention and require calling
        # revert, like for properties.
        gclient_utils.CheckCall(
            ['svn', 'revert', file_status[1]], cwd=repo_root)
      else:
        if not os.path.exists(file_path):
          pass
        elif os.path.isfile(file_path) or os.path.islink(file_path):
          logging.info('os.remove(%s)' % file_path)
          os.remove(file_path)
        elif os.path.isdir(file_path):
          logging.info('gclient_utils.RemoveDirectory(%s)' % file_path)
          gclient_utils.RemoveDirectory(file_path)
        else:
          logging.critical(
            ('No idea what is %s.\nYou just found a bug in gclient'
             ', please ping maruel@chromium.org ASAP!') % file_path)
