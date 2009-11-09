#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Client-side script to send a try job to the try server. It communicates to
the try server by either writting to a svn repository or by directly connecting
to the server by HTTP.
"""


import datetime
import getpass
import logging
import optparse
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import traceback
import urllib

import gcl
import gclient
import gclient_scm
import presubmit_support
import upload

__version__ = '1.1.1'


# Constants
HELP_STRING = "Sorry, Tryserver is not available."
USAGE = r"""%prog [change_name] [options]

Client-side script to send a try job to the try server. It communicates to
the try server by either writting to a svn repository or by directly connecting
to the server by HTTP.


Examples:
  Try a change against a particular revision:
    %prog change_name -r 123

  A git patch off a web site (git inserts a/ and b/) and fix the base dir:
    %prog --url http://url/to/patch.diff --patchlevel 1 --root src

  Use svn to store the try job, specify an alternate email address and use a
  premade diff file on the local drive:
    %prog --email user@example.com
            --svn_repo svn://svn.chromium.org/chrome-try/try --diff foo.diff

  Running only on a 'mac' slave with revision 123 and clobber first; specify
  manually the 3 source files to use for the try job:
    %prog --bot mac --revision 123 --clobber -f src/a.cc -f src/a.h
            -f include/b.h

"""

class InvalidScript(Exception):
  def __str__(self):
    return self.args[0] + '\n' + HELP_STRING


class NoTryServerAccess(Exception):
  def __str__(self):
    return self.args[0] + '\n' + HELP_STRING


def PathDifference(root, subpath):
  """Returns the difference subpath minus root."""
  if subpath.find(root) != 0:
    return None
  # If the root does not have a trailing \ or /, we add it so the returned path
  # starts immediately after the seperator regardless of whether it is provided.
  if not root.endswith(os.sep):
    root += os.sep
  return subpath[len(root):]


def GetSourceRoot():
  """Returns the absolute directory one level up from the repository root."""
  # TODO(maruel): This is odd to assume that '..' is the source root.
  return os.path.abspath(os.path.join(gcl.GetRepositoryRoot(), '..'))


def GetTryServerSettings():
  """Grab try server settings local to the repository."""
  def _SafeResolve(host):
    try:
      return socket.getaddrinfo(host, None)
    except socket.gaierror:
      return None

  settings = {}
  settings['http_port'] = gcl.GetCodeReviewSetting('TRYSERVER_HTTP_PORT')
  settings['http_host'] = gcl.GetCodeReviewSetting('TRYSERVER_HTTP_HOST')
  settings['svn_repo'] = gcl.GetCodeReviewSetting('TRYSERVER_SVN_URL')
  settings['default_project'] = gcl.GetCodeReviewSetting('TRYSERVER_PROJECT')
  settings['default_root'] = gcl.GetCodeReviewSetting('TRYSERVER_ROOT')

  # Pick a patchlevel, default to 0.
  default_patchlevel = gcl.GetCodeReviewSetting('TRYSERVER_PATCHLEVEL')
  if default_patchlevel:
    default_patchlevel = int(default_patchlevel)
  else:
    default_patchlevel = 0
  settings['default_patchlevel'] = default_patchlevel

  # Use http is the http_host name resolve, fallback to svn otherwise.
  if (settings['http_port'] and settings['http_host'] and
      _SafeResolve(settings['http_host'])):
    settings['default_transport'] = 'http'
  elif settings.get('svn_repo'):
    settings['default_transport'] = 'svn'
  return settings


def EscapeDot(name):
  return name.replace('.', '-')


def RunCommand(command):
  output, retcode = gcl.RunShellWithReturnCode(command)
  if retcode:
    raise NoTryServerAccess(' '.join(command) + '\nOuput:\n' + output)
  return output


class SCM(object):
  """Simplistic base class to implement one function: ProcessOptions."""
  def __init__(self, options):
    self.options = options

  def ProcessOptions(self):
    raise Unimplemented


class SVN(SCM):
  """Gathers the options and diff for a subversion checkout."""
  def GenerateDiff(self, files, root):
    """Returns a string containing the diff for the given file list.

    The files in the list should either be absolute paths or relative to the
    given root. If no root directory is provided, the repository root will be
    used.
    """
    previous_cwd = os.getcwd()
    if root is None:
      os.chdir(gcl.GetRepositoryRoot())
    else:
      os.chdir(root)

    diff = []
    for file in files:
      # Use svn info output instead of os.path.isdir because the latter fails
      # when the file is deleted.
      if gclient_scm.CaptureSVNInfo(file).get("Node Kind") in ("dir",
                                                               "directory"):
        continue
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
      data = gcl.RunShell(["svn", "diff", "--config-dir", bogus_dir, file])

      # We know the diff will be incorrectly formatted. Fix it.
      if gcl.IsSVNMoved(file):
        # The file is "new" in the patch sense. Generate a homebrew diff.
        # We can't use ReadFile() since it's not using binary mode.
        file_handle = open(file, 'rb')
        file_content = file_handle.read()
        file_handle.close()
        # Prepend '+' to every lines.
        file_content = ['+' + i for i in file_content.splitlines(True)]
        nb_lines = len(file_content)
        # We need to use / since patch on unix will fail otherwise.
        file = file.replace('\\', '/')
        data = "Index: %s\n" % file
        data += ("============================================================="
                 "======\n")
        # Note: Should we use /dev/null instead?
        data += "--- %s\n" % file
        data += "+++ %s\n" % file
        data += "@@ -0,0 +1,%d @@\n" % nb_lines
        data += ''.join(file_content)
      diff.append(data)
    os.chdir(previous_cwd)
    return "".join(diff)

  def GetFileNames(self):
    """Return the list of files in the diff."""
    return self.change_info.GetFileNames()

  def GetLocalRoot(self):
    """Return the path of the repository root."""
    return self.change_info.GetLocalRoot()

  def ProcessOptions(self):
    if not self.options.diff:
      # Generate the diff with svn and write it to the submit queue path.  The
      # files are relative to the repository root, but we need patches relative
      # to one level up from there (i.e., 'src'), so adjust both the file
      # paths and the root of the diff.
      source_root = GetSourceRoot()
      prefix = PathDifference(source_root, gcl.GetRepositoryRoot())
      adjusted_paths = [os.path.join(prefix, x) for x in self.options.files]
      self.options.diff = self.GenerateDiff(adjusted_paths, root=source_root)
      self.change_info = gcl.LoadChangelistInfoForMultiple(self.options.name,
          gcl.GetRepositoryRoot(), True, True)


class GIT(SCM):
  """Gathers the options and diff for a git checkout."""
  def GenerateDiff(self):
    """Get the diff we'll send to the try server. We ignore the files list."""
    branch = upload.RunShell(['git', 'cl', 'upstream']).strip()
    diff = upload.RunShell(['git', 'diff-tree', '-p', '--no-prefix',
                            branch, 'HEAD']).splitlines(True)
    for i in range(len(diff)):
      # In the case of added files, replace /dev/null with the path to the
      # file being added.
      if diff[i].startswith('--- /dev/null'):
        diff[i] = '--- %s' % diff[i+1][4:]
    return ''.join(diff)

  def GetEmail(self):
    # TODO: check for errors here?
    return upload.RunShell(['git', 'config', 'user.email']).strip()

  def GetFileNames(self):
    """Return the list of files in the diff."""
    return self.options.files

  def GetLocalRoot(self):
    """Return the path of the repository root."""
    # TODO: check for errors here?
    root = upload.RunShell(['git', 'rev-parse', '--show-cdup']).strip()
    return os.path.abspath(root)

  def GetPatchName(self):
    """Construct a name for this patch."""
    # TODO: perhaps include the hash of the current commit, to distinguish
    # patches?
    branch = upload.RunShell(['git', 'symbolic-ref', 'HEAD']).strip()
    if not branch.startswith('refs/heads/'):
      raise "Couldn't figure out branch name"
    branch = branch[len('refs/heads/'):]
    return branch

  def ProcessOptions(self):
    if not self.options.diff:
      self.options.diff = self.GenerateDiff()
    if not self.options.name:
      self.options.name = self.GetPatchName()
    if not self.options.email:
      self.options.email = self.GetEmail()


def _ParseSendChangeOptions(options):
  """Parse common options passed to _SendChangeHTTP and _SendChangeSVN."""
  values = {}
  if options.email:
    values['email'] = options.email
  values['user'] = options.user
  values['name'] = options.name
  if options.bot:
    values['bot'] = ','.join(options.bot)
  if options.revision:
    values['revision'] = options.revision
  if options.clobber:
    values['clobber'] = 'true'
  if options.tests:
    values['tests'] = ','.join(options.tests)
  if options.root:
    values['root'] = options.root
  if options.patchlevel:
    values['patchlevel'] = options.patchlevel
  if options.issue:
    values['issue'] = options.issue
  if options.patchset:
    values['patchset'] = options.patchset
  if options.target:
    values['target'] = options.target
  if options.project:
    values['project'] = options.project
  return values


def _SendChangeHTTP(options):
  """Send a change to the try server using the HTTP protocol."""
  if not options.host:
    raise NoTryServerAccess('Please use the --host option to specify the try '
        'server host to connect to.')
  if not options.port:
    raise NoTryServerAccess('Please use the --port option to specify the try '
        'server port to connect to.')

  values = _ParseSendChangeOptions(options)
  values['patch'] = options.diff

  url = 'http://%s:%s/send_try_patch' % (options.host, options.port)
  proxies = None
  if options.proxy:
    if options.proxy.lower() == 'none':
      # Effectively disable HTTP_PROXY or Internet settings proxy setup.
      proxies = {}
    else:
      proxies = {'http': options.proxy, 'https': options.proxy}
  try:
    connection = urllib.urlopen(url, urllib.urlencode(values), proxies=proxies)
  except IOError, e:
    if (values.get('bot') and len(e.args) > 2 and
        e.args[2] == 'got a bad status line'):
      raise NoTryServerAccess('%s is unaccessible. Bad --bot argument?' % url)
    else:
      raise NoTryServerAccess('%s is unaccessible. Reason: %s' % (url,
                                                                  str(e.args)))
  if not connection:
    raise NoTryServerAccess('%s is unaccessible.' % url)
  if connection.read() != 'OK':
    raise NoTryServerAccess('%s is unaccessible.' % url)


def _SendChangeSVN(options):
  """Send a change to the try server by committing a diff file on a subversion
  server."""
  if not options.svn_repo:
    raise NoTryServerAccess('Please use the --svn_repo option to specify the'
                            ' try server svn repository to connect to.')

  values = _ParseSendChangeOptions(options)
  description = ''
  for (k,v) in values.iteritems():
    description += "%s=%s\n" % (k,v)

  # Do an empty checkout.
  temp_dir = tempfile.mkdtemp()
  temp_file = tempfile.NamedTemporaryFile()
  temp_file_name = temp_file.name
  try:
    # Don't use  '--non-interactive', since we want it to prompt for
    # crendentials if necessary.
    command = ['svn', 'checkout', '--depth', 'empty', '-q',
               options.svn_repo, temp_dir]
    if options.email:
      command += ['--username', options.email]
    # Don't use RunCommand() since svn may prompt for information.
    use_shell = sys.platform.startswith("win")
    subprocess.Popen(command, shell=use_shell).communicate()

    # TODO(maruel): Use a subdirectory per user?
    current_time = str(datetime.datetime.now()).replace(':', '.')
    file_name = (EscapeDot(options.user) + '.' + EscapeDot(options.name) +
                 '.%s.diff' % current_time)
    full_path = os.path.join(temp_dir, file_name)
    full_url = options.svn_repo + '/' + file_name
    file_found = False
    try:
      RunCommand(['svn', 'ls', full_url])
      file_found = True
    except NoTryServerAccess:
      pass
    if file_found:
      # The file already exists in the repo. Note that commiting a file is a
      # no-op if the file's content (the diff) is not modified. This is why the
      # file name contains the date and time.
      RunCommand(['svn', 'update', full_path])
      file = open(full_path, 'wb')
      file.write(options.diff)
      file.close()
    else:
      # Add the file to the repo
      file = open(full_path, 'wb')
      file.write(options.diff)
      file.close()
      RunCommand(["svn", "add", full_path])
    temp_file.write(description)
    temp_file.flush()
    RunCommand(["svn", "commit", full_path, '--file',
                temp_file_name])
  finally:
    temp_file.close()
    shutil.rmtree(temp_dir, True)


def GuessVCS(options):
  """Helper to guess the version control system.

  NOTE: Very similar to upload.GuessVCS. Doesn't look for hg since we don't
  support it yet.

  This examines the current directory, guesses which SCM we're using, and
  returns an instance of the appropriate class.  Exit with an error if we can't
  figure it out.

  Returns:
    A SCM instance. Exits if the SCM can't be guessed.
  """
  # Subversion has a .svn in all working directories.
  if os.path.isdir('.svn'):
    logging.info("Guessed VCS = Subversion")
    return SVN(options)

  # Git has a command to test if you're in a git tree.
  # Try running it, but don't die if we don't have git installed.
  try:
    out, returncode = gcl.RunShellWithReturnCode(["git", "rev-parse",
                                                  "--is-inside-work-tree"])
    if returncode == 0:
      logging.info("Guessed VCS = Git")
      return GIT(options)
  except OSError, (errno, message):
    if errno != 2:  # ENOENT -- they don't have git installed.
      raise

  raise NoTryServerAccess("Could not guess version control system. "
                          "Are you in a working copy directory?")


def TryChange(argv,
              file_list,
              swallow_exception,
              prog=None):
  """
  Args:
    argv: Arguments and options.
    file_list: Default value to pass to --file.
    swallow_exception: Whether we raise or swallow exceptions.
  """
  default_settings = GetTryServerSettings()
  transport_functions = { 'http': _SendChangeHTTP, 'svn': _SendChangeSVN }
  default_transport = transport_functions.get(
      default_settings.get('default_transport'))

  # Parse argv
  parser = optparse.OptionParser(usage=USAGE,
                                 version=__version__,
                                 prog=prog)

  group = optparse.OptionGroup(parser, "Result and status")
  group.add_option("-u", "--user", default=getpass.getuser(),
                   help="Owner user name [default: %default]")
  group.add_option("-e", "--email",
                   default=os.environ.get('TRYBOT_RESULTS_EMAIL_ADDRESS',
                        os.environ.get('EMAIL_ADDRESS')),
                   help="Email address where to send the results. Use either "
                        "the TRYBOT_RESULTS_EMAIL_ADDRESS environment "
                        "variable or EMAIL_ADDRESS to set the email address "
                        "the try bots report results to [default: %default]")
  group.add_option("-n", "--name",
                   help="Descriptive name of the try job")
  group.add_option("--issue", type='int',
                   help="Update rietveld issue try job status")
  group.add_option("--patchset", type='int',
                   help="Update rietveld issue try job status")
  parser.add_option_group(group)

  group = optparse.OptionGroup(parser, "Try job options")
  group.add_option("-b", "--bot", action="append",
                    help="Only use specifics build slaves, ex: '--bot win' to "
                         "run the try job only on the 'win' slave; see the try "
                         "server waterfall for the slave's name")
  group.add_option("-r", "--revision",
                    help="Revision to use for the try job; default: the "
                         "revision will be determined by the try server; see "
                         "its waterfall for more info")
  group.add_option("-c", "--clobber", action="store_true",
                    help="Force a clobber before building; e.g. don't do an "
                         "incremental build")
  # TODO(maruel): help="Select a specific configuration, usually 'debug' or "
  #                    "'release'"
  group.add_option("--target", help=optparse.SUPPRESS_HELP)

  # TODO(bradnelson): help="Override which project to use"
  group.add_option("--project", help=optparse.SUPPRESS_HELP,
                   default=default_settings['default_project'])

  # Override the list of tests to run, use multiple times to list many tests
  # (or comma separated)
  group.add_option("-t", "--tests", action="append",
                   help=optparse.SUPPRESS_HELP)
  parser.add_option_group(group)

  group = optparse.OptionGroup(parser, "Patch to run")
  group.add_option("-f", "--file", default=file_list, dest="files",
                   metavar="FILE", action="append",
                   help="Use many times to list the files to include in the "
                        "try, relative to the repository root")
  group.add_option("--diff",
                   help="File containing the diff to try")
  group.add_option("--url",
                   help="Url where to grab a patch")
  group.add_option("--root",
                   help="Root to use for the patch; base subdirectory for "
                        "patch created in a subdirectory",
                   default=default_settings["default_root"])
  group.add_option("--patchlevel", type='int', metavar="LEVEL",
                   help="Used as -pN parameter to patch",
                   default=default_settings["default_patchlevel"])
  parser.add_option_group(group)

  group = optparse.OptionGroup(parser, "Access the try server by HTTP")
  group.add_option("--use_http",
                   action="store_const",
                   const=_SendChangeHTTP,
                   dest="send_patch",
                   default=default_transport,
                   help="Use HTTP to talk to the try server [default]")
  group.add_option("--host",
                   default=default_settings['http_host'],
                   help="Host address")
  group.add_option("--port",
                   default=default_settings['http_port'],
                   help="HTTP port")
  group.add_option("--proxy",
                   help="HTTP proxy")
  parser.add_option_group(group)

  group = optparse.OptionGroup(parser, "Access the try server with SVN")
  group.add_option("--use_svn",
                   action="store_const",
                   const=_SendChangeSVN,
                   dest="send_patch",
                   help="Use SVN to talk to the try server")
  group.add_option("--svn_repo",
                   metavar="SVN_URL",
                   default=default_settings['svn_repo'],
                   help="SVN url to use to write the changes in; --use_svn is "
                        "implied when using --svn_repo")
  parser.add_option_group(group)

  options, args = parser.parse_args(argv)

  # Switch the default accordingly if there was no default send_patch.
  if not options.send_patch:
    if options.port and options.host:
      options.send_patch = _SendChangeHTTP
    elif options.svn_repo:
      options.send_patch = _SendChangeSVN

  if len(args) == 1 and args[0] == 'help':
    parser.print_help()
  if (not options.files and (not options.issue and options.patchset) and
      not options.diff and not options.url):
    # TODO(maruel): It should just try the modified files showing up in a
    # svn status.
    print "Nothing to try, changelist is empty."
    return 1

  try:
    # Convert options.diff into the content of the diff.
    if options.url:
      options.diff = urllib.urlopen(options.url).read()
    elif options.diff:
      options.diff = gcl.ReadFile(options.diff)
    # Process the VCS in any case at least to retrieve the email address.
    try:
      options.scm = GuessVCS(options)
      options.scm.ProcessOptions()
    except NoTryServerAccess, e:
      # If we got the diff, we don't care.
      if not options.diff:
        raise

    # Get try slaves from PRESUBMIT.py files if not specified.
    if not options.bot:
      if options.url:
        print('You need to specify which bots to use.')
        return 1
      root_presubmit = gcl.GetCachedFile('PRESUBMIT.py', use_root=True)
      options.bot = presubmit_support.DoGetTrySlaves(options.scm.GetFileNames(),
                                                     options.scm.GetLocalRoot(),
                                                     root_presubmit,
                                                     False,
                                                     sys.stdout)

    if options.name is None:
      if options.issue:
        patch_name = 'Issue %s' % options.issue
      else:
        options.name = 'Unnamed'
        print('Note: use --name NAME to change the try job name.')
    if not options.email:
      print('Warning: TRYBOT_RESULTS_EMAIL_ADDRESS is not set. Try server '
            'results might\ngo to: %s@google.com.\n' % options.user)
    else:
      print('Results will be emailed to: ' + options.email)

    # Send the patch.
    options.send_patch(options)
    print 'Patch \'%s\' sent to try server: %s' % (options.name,
                                                   ', '.join(options.bot))
  except (InvalidScript, NoTryServerAccess), e:
    if swallow_exception:
      return 1
    print e
    return 1
  return 0


if __name__ == "__main__":
  sys.exit(TryChange(None, None, False))
