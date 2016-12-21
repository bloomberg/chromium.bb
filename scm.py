# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""SCM-specific utility classes."""

import cStringIO
import glob
import logging
import os
import re
import sys
import tempfile
import time
from xml.etree import ElementTree

import gclient_utils
import subprocess2


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


def determine_scm(root):
  """Similar to upload.py's version but much simpler.

  Returns 'svn', 'git' or None.
  """
  if os.path.isdir(os.path.join(root, '.svn')):
    return 'svn'
  elif os.path.isdir(os.path.join(root, '.git')):
    return 'git'
  else:
    try:
      subprocess2.check_call(
          ['git', 'rev-parse', '--show-cdup'],
          stdout=subprocess2.VOID,
          stderr=subprocess2.VOID,
          cwd=root)
      return 'git'
    except (OSError, subprocess2.CalledProcessError):
      return None


def only_int(val):
  if val.isdigit():
    return int(val)
  else:
    return 0


class GIT(object):
  current_version = None

  @staticmethod
  def ApplyEnvVars(kwargs):
    env = kwargs.pop('env', None) or os.environ.copy()
    # Don't prompt for passwords; just fail quickly and noisily.
    # By default, git will use an interactive terminal prompt when a username/
    # password is needed.  That shouldn't happen in the chromium workflow,
    # and if it does, then gclient may hide the prompt in the midst of a flood
    # of terminal spew.  The only indication that something has gone wrong
    # will be when gclient hangs unresponsively.  Instead, we disable the
    # password prompt and simply allow git to fail noisily.  The error
    # message produced by git will be copied to gclient's output.
    env.setdefault('GIT_ASKPASS', 'true')
    env.setdefault('SSH_ASKPASS', 'true')
    # 'cat' is a magical git string that disables pagers on all platforms.
    env.setdefault('GIT_PAGER', 'cat')
    return env

  @staticmethod
  def Capture(args, cwd, strip_out=True, **kwargs):
    env = GIT.ApplyEnvVars(kwargs)
    output = subprocess2.check_output(
        ['git'] + args,
        cwd=cwd, stderr=subprocess2.PIPE, env=env, **kwargs)
    return output.strip() if strip_out else output

  @staticmethod
  def CaptureStatus(files, cwd, upstream_branch):
    """Returns git status.

    @files can be a string (one file) or a list of files.

    Returns an array of (status, file) tuples."""
    if upstream_branch is None:
      upstream_branch = GIT.GetUpstreamBranch(cwd)
      if upstream_branch is None:
        raise gclient_utils.Error('Cannot determine upstream branch')
    command = ['diff', '--name-status', '--no-renames',
               '-r', '%s...' % upstream_branch]
    if not files:
      pass
    elif isinstance(files, basestring):
      command.append(files)
    else:
      command.extend(files)
    status = GIT.Capture(command, cwd)
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
  def IsWorkTreeDirty(cwd):
    return GIT.Capture(['status', '-s'], cwd=cwd) != ''

  @staticmethod
  def GetEmail(cwd):
    """Retrieves the user email address if known."""
    # We could want to look at the svn cred when it has a svn remote but it
    # should be fine for now, users should simply configure their git settings.
    try:
      return GIT.Capture(['config', 'user.email'], cwd=cwd)
    except subprocess2.CalledProcessError:
      return ''

  @staticmethod
  def ShortBranchName(branch):
    """Converts a name like 'refs/heads/foo' to just 'foo'."""
    return branch.replace('refs/heads/', '')

  @staticmethod
  def GetBranchRef(cwd):
    """Returns the full branch reference, e.g. 'refs/heads/master'."""
    return GIT.Capture(['symbolic-ref', 'HEAD'], cwd=cwd)

  @staticmethod
  def GetBranch(cwd):
    """Returns the short branch name, e.g. 'master'."""
    return GIT.ShortBranchName(GIT.GetBranchRef(cwd))

  @staticmethod
  def IsGitSvn(cwd):
    """Returns True if this repo looks like it's using git-svn."""
    # If you have any "svn-remote.*" config keys, we think you're using svn.
    try:
      GIT.Capture(['config', '--local', '--get-regexp', r'^svn-remote\.'],
                  cwd=cwd)
      return True
    except subprocess2.CalledProcessError:
      return False

  @staticmethod
  def MatchSvnGlob(url, base_url, glob_spec, allow_wildcards):
    """Return the corresponding git ref if |base_url| together with |glob_spec|
    matches the full |url|.

    If |allow_wildcards| is true, |glob_spec| can contain wildcards (see below).
    """
    fetch_suburl, as_ref = glob_spec.split(':')
    if allow_wildcards:
      glob_match = re.match('(.+/)?(\*|{[^/]*})(/.+)?', fetch_suburl)
      if glob_match:
        # Parse specs like "branches/*/src:refs/remotes/svn/*" or
        # "branches/{472,597,648}/src:refs/remotes/svn/*".
        branch_re = re.escape(base_url)
        if glob_match.group(1):
          branch_re += '/' + re.escape(glob_match.group(1))
        wildcard = glob_match.group(2)
        if wildcard == '*':
          branch_re += '([^/]*)'
        else:
          # Escape and replace surrounding braces with parentheses and commas
          # with pipe symbols.
          wildcard = re.escape(wildcard)
          wildcard = re.sub('^\\\\{', '(', wildcard)
          wildcard = re.sub('\\\\,', '|', wildcard)
          wildcard = re.sub('\\\\}$', ')', wildcard)
          branch_re += wildcard
        if glob_match.group(3):
          branch_re += re.escape(glob_match.group(3))
        match = re.match(branch_re, url)
        if match:
          return re.sub('\*$', match.group(1), as_ref)

    # Parse specs like "trunk/src:refs/remotes/origin/trunk".
    if fetch_suburl:
      full_url = base_url + '/' + fetch_suburl
    else:
      full_url = base_url
    if full_url == url:
      return as_ref
    return None

  @staticmethod
  def GetSVNBranch(cwd):
    """Returns the svn branch name if found."""
    # Try to figure out which remote branch we're based on.
    # Strategy:
    # 1) iterate through our branch history and find the svn URL.
    # 2) find the svn-remote that fetches from the URL.

    # regexp matching the git-svn line that contains the URL.
    git_svn_re = re.compile(r'^\s*git-svn-id: (\S+)@', re.MULTILINE)

    # We don't want to go through all of history, so read a line from the
    # pipe at a time.
    # The -100 is an arbitrary limit so we don't search forever.
    cmd = ['git', 'log', '-100', '--pretty=medium']
    proc = subprocess2.Popen(cmd, cwd=cwd, stdout=subprocess2.PIPE)
    url = None
    for line in proc.stdout:
      match = git_svn_re.match(line)
      if match:
        url = match.group(1)
        proc.stdout.close()  # Cut pipe.
        break

    if url:
      svn_remote_re = re.compile(r'^svn-remote\.([^.]+)\.url (.*)$')
      remotes = GIT.Capture(
          ['config', '--local', '--get-regexp', r'^svn-remote\..*\.url'],
          cwd=cwd).splitlines()
      for remote in remotes:
        match = svn_remote_re.match(remote)
        if match:
          remote = match.group(1)
          base_url = match.group(2)
          try:
            fetch_spec = GIT.Capture(
                ['config', '--local', 'svn-remote.%s.fetch' % remote],
                cwd=cwd)
            branch = GIT.MatchSvnGlob(url, base_url, fetch_spec, False)
          except subprocess2.CalledProcessError:
            branch = None
          if branch:
            return branch
          try:
            branch_spec = GIT.Capture(
                ['config', '--local', 'svn-remote.%s.branches' % remote],
                cwd=cwd)
            branch = GIT.MatchSvnGlob(url, base_url, branch_spec, True)
          except subprocess2.CalledProcessError:
            branch = None
          if branch:
            return branch
          try:
            tag_spec = GIT.Capture(
                ['config', '--local', 'svn-remote.%s.tags' % remote],
                cwd=cwd)
            branch = GIT.MatchSvnGlob(url, base_url, tag_spec, True)
          except subprocess2.CalledProcessError:
            branch = None
          if branch:
            return branch

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
          ['config', '--local', 'branch.%s.merge' % branch], cwd=cwd)
    except subprocess2.CalledProcessError:
      upstream_branch = None
    if upstream_branch:
      try:
        remote = GIT.Capture(
            ['config', '--local', 'branch.%s.remote' % branch], cwd=cwd)
      except subprocess2.CalledProcessError:
        pass
    else:
      try:
        upstream_branch = GIT.Capture(
            ['config', '--local', 'rietveld.upstream-branch'], cwd=cwd)
      except subprocess2.CalledProcessError:
        upstream_branch = None
      if upstream_branch:
        try:
          remote = GIT.Capture(
              ['config', '--local', 'rietveld.upstream-remote'], cwd=cwd)
        except subprocess2.CalledProcessError:
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
  def RefToRemoteRef(ref, remote=None):
    """Convert a checkout ref to the equivalent remote ref.

    Returns:
      A tuple of the remote ref's (common prefix, unique suffix), or None if it
      doesn't appear to refer to a remote ref (e.g. it's a commit hash).
    """
    # TODO(mmoss): This is just a brute-force mapping based of the expected git
    # config. It's a bit better than the even more brute-force replace('heads',
    # ...), but could still be smarter (like maybe actually using values gleaned
    # from the git config).
    m = re.match('^(refs/(remotes/)?)?branch-heads/', ref or '')
    if m:
      return ('refs/remotes/branch-heads/', ref.replace(m.group(0), ''))
    if remote:
      m = re.match('^((refs/)?remotes/)?%s/|(refs/)?heads/' % remote, ref or '')
      if m:
        return ('refs/remotes/%s/' % remote, ref.replace(m.group(0), ''))
    return None

  @staticmethod
  def GetUpstreamBranch(cwd):
    """Gets the current branch's upstream branch."""
    remote, upstream_branch = GIT.FetchUpstreamTuple(cwd)
    if remote != '.' and upstream_branch:
      remote_ref = GIT.RefToRemoteRef(upstream_branch, remote)
      if remote_ref:
        upstream_branch = ''.join(remote_ref)
    return upstream_branch

  @staticmethod
  def GenerateDiff(cwd, branch=None, branch_head='HEAD', full_move=False,
                   files=None):
    """Diffs against the upstream branch or optionally another branch.

    full_move means that move or copy operations should completely recreate the
    files, usually in the prospect to apply the patch for a try job."""
    if not branch:
      branch = GIT.GetUpstreamBranch(cwd)
    command = ['diff', '-p', '--no-color', '--no-prefix', '--no-ext-diff',
               branch + "..." + branch_head]
    if full_move:
      command.append('--no-renames')
    else:
      command.append('-C')
    # TODO(maruel): --binary support.
    if files:
      command.append('--')
      command.extend(files)
    diff = GIT.Capture(command, cwd=cwd, strip_out=False).splitlines(True)
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
    short_sha = GIT.Capture(['rev-parse', '--short=4', 'HEAD'], cwd=cwd)
    return "%s#%s" % (GIT.GetBranch(cwd), short_sha)

  @staticmethod
  def GetCheckoutRoot(cwd):
    """Returns the top level directory of a git checkout as an absolute path.
    """
    root = GIT.Capture(['rev-parse', '--show-cdup'], cwd=cwd)
    return os.path.abspath(os.path.join(cwd, root))

  @staticmethod
  def GetGitDir(cwd):
    return os.path.abspath(GIT.Capture(['rev-parse', '--git-dir'], cwd=cwd))

  @staticmethod
  def IsInsideWorkTree(cwd):
    try:
      return GIT.Capture(['rev-parse', '--is-inside-work-tree'], cwd=cwd)
    except (OSError, subprocess2.CalledProcessError):
      return False

  @staticmethod
  def IsDirectoryVersioned(cwd, relative_dir):
    """Checks whether the given |relative_dir| is part of cwd's repo."""
    return bool(GIT.Capture(['ls-tree', 'HEAD', relative_dir], cwd=cwd))

  @staticmethod
  def CleanupDir(cwd, relative_dir):
    """Cleans up untracked file inside |relative_dir|."""
    return bool(GIT.Capture(['clean', '-df', relative_dir], cwd=cwd))

  @staticmethod
  def GetGitSvnHeadRev(cwd):
    """Gets the most recently pulled git-svn revision."""
    try:
      output = GIT.Capture(['svn', 'info'], cwd=cwd)
      match = re.search(r'^Revision: ([0-9]+)$', output, re.MULTILINE)
      return int(match.group(1)) if match else None
    except (subprocess2.CalledProcessError, ValueError):
      return None

  @staticmethod
  def ParseGitSvnSha1(output):
    """Parses git-svn output for the first sha1."""
    match = re.search(r'[0-9a-fA-F]{40}', output)
    return match.group(0) if match else None

  @staticmethod
  def GetSha1ForSvnRev(cwd, rev):
    """Returns a corresponding git sha1 for a SVN revision."""
    if not GIT.IsGitSvn(cwd=cwd):
      return None
    try:
      output = GIT.Capture(['svn', 'find-rev', 'r' + str(rev)], cwd=cwd)
      return GIT.ParseGitSvnSha1(output)
    except subprocess2.CalledProcessError:
      return None

  @staticmethod
  def GetBlessedSha1ForSvnRev(cwd, rev):
    """Returns a git commit hash from the master branch history that has
    accurate .DEPS.git and git submodules.  To understand why this is more
    complicated than a simple call to `git svn find-rev`, refer to:

    http://www.chromium.org/developers/how-tos/git-repo
    """
    git_svn_rev = GIT.GetSha1ForSvnRev(cwd, rev)
    if not git_svn_rev:
      return None
    try:
      output = GIT.Capture(
          ['rev-list', '--ancestry-path', '--reverse',
          '--grep', 'SVN changes up to revision [0-9]*',
          '%s..refs/remotes/origin/master' % git_svn_rev], cwd=cwd)
      if not output:
        return None
      sha1 = output.splitlines()[0]
      if not sha1:
        return None
      output = GIT.Capture(['rev-list', '-n', '1', '%s^1' % sha1], cwd=cwd)
      if git_svn_rev != output.rstrip():
        raise gclient_utils.Error(sha1)
      return sha1
    except subprocess2.CalledProcessError:
      return None

  @staticmethod
  def IsValidRevision(cwd, rev, sha_only=False):
    """Verifies the revision is a proper git revision.

    sha_only: Fail unless rev is a sha hash.
    """
    # 'git rev-parse foo' where foo is *any* 40 character hex string will return
    # the string and return code 0. So strip one character to force 'git
    # rev-parse' to do a hash table look-up and returns 128 if the hash is not
    # present.
    lookup_rev = rev
    if re.match(r'^[0-9a-fA-F]{40}$', rev):
      lookup_rev = rev[:-1]
    try:
      sha = GIT.Capture(['rev-parse', lookup_rev], cwd=cwd).lower()
      if lookup_rev != rev:
        # Make sure we get the original 40 chars back.
        return rev.lower() == sha
      if sha_only:
        return sha.startswith(rev.lower())
      return True
    except subprocess2.CalledProcessError:
      return False

  @classmethod
  def AssertVersion(cls, min_version):
    """Asserts git's version is at least min_version."""
    if cls.current_version is None:
      current_version = cls.Capture(['--version'], '.')
      matched = re.search(r'version ([0-9\.]+)', current_version)
      cls.current_version = matched.group(1)
    current_version_list = map(only_int, cls.current_version.split('.'))
    for min_ver in map(int, min_version.split('.')):
      ver = current_version_list.pop(0)
      if ver < min_ver:
        return (False, cls.current_version)
      elif ver > min_ver:
        return (True, cls.current_version)
    return (True, cls.current_version)
