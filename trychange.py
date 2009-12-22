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
import urllib

try:
  import breakpad
except ImportError:
  pass

import gclient_utils
import scm

__version__ = '1.2'


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
            -f include/b.h"""

class InvalidScript(Exception):
  def __str__(self):
    return self.args[0] + '\n' + HELP_STRING


class NoTryServerAccess(Exception):
  def __str__(self):
    return self.args[0] + '\n' + HELP_STRING


def EscapeDot(name):
  return name.replace('.', '-')


class SCM(object):
  """Simplistic base class to implement one function: ProcessOptions."""
  def __init__(self, options):
    self.options = options

  def GetFileNames(self):
    """Return the list of files in the diff."""
    return self.options.files


class SVN(SCM):
  """Gathers the options and diff for a subversion checkout."""
  def __init__(self, *args, **kwargs):
    SCM.__init__(self, *args, **kwargs)
    self.checkout_root = scm.SVN.GetCheckoutRoot(os.getcwd())
    if not self.options.diff:
      # Generate the diff from the scm.
      self.options.diff = self._GenerateDiff()
    if not self.options.email:
      # Assumes the svn credential is an email address.
      self.options.email = scm.SVN.GetEmail(self.checkout_root)

  def _GenerateDiff(self):
    """Returns a string containing the diff for the given file list.

    The files in the list should either be absolute paths or relative to the
    given root.
    """
    if not self.options.files:
      previous_cwd = os.getcwd()
      os.chdir(self.checkout_root)
      excluded = ['!', '?', 'X', ' ', '~']
      self.options.files = [
          f[1] for f in scm.SVN.CaptureStatus(self.checkout_root)
          if f[0][0] not in excluded
      ]
      os.chdir(previous_cwd)
    return scm.SVN.GenerateDiff(self.options.files, full_move=True)

  def GetLocalRoot(self):
    """Return the path of the repository root."""
    return self.checkout_root

  def GetBots(self):
    try:
      import gcl
      return gcl.GetCachedFile('PRESUBMIT.py', use_root=True)
    except ImportError:
      try:
        return gclient_utils.FileRead(os.path.join(self.checkout_root,
                                                   'PRESUBMIT.py'))
      except (IOError, OSError):
        return None


class GIT(SCM):
  """Gathers the options and diff for a git checkout."""
  def __init__(self, *args, **kwargs):
    SCM.__init__(self, *args, **kwargs)
    self.checkout_root = scm.GIT.GetCheckoutRoot(os.getcwd())
    if not self.options.diff:
      self.options.diff = scm.GIT.GenerateDiff(self.checkout_root)
    if not self.options.name:
      self.options.name = scm.GIT.GetPatchName(self.checkout_root)
    if not self.options.email:
      self.options.email = scm.GIT.GetEmail(self.checkout_root)

  def GetLocalRoot(self):
    """Return the path of the repository root."""
    return self.checkout_root

  def GetBots(self):
    try:
      return gclient_utils.FileRead(os.path.join(self.checkout_root,
                                                 'PRESUBMIT.py'))
    except (IOError, OSError):
      return None


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
  try:
    try:
      command = ['svn', 'checkout', '--depth', 'empty', '-q',
                 options.svn_repo, temp_dir]
      if options.email:
        command.extend(['--username', options.email])
      gclient_utils.CheckCall(command)

      # TODO(maruel): Use a subdirectory per user?
      current_time = str(datetime.datetime.now()).replace(':', '.')
      file_name = (EscapeDot(options.user) + '.' + EscapeDot(options.name) +
                   '.%s.diff' % current_time)
      full_path = os.path.join(temp_dir, file_name)
      full_url = options.svn_repo + '/' + file_name
      file_found = False
      try:
        gclient_utils.CheckCall(['svn', 'ls', full_url], print_error=False)
        file_found = True
      except gclient_utils.CheckCallError:
        pass
      if file_found:
        # The file already exists in the repo. Note that commiting a file is a
        # no-op if the file's content (the diff) is not modified. This is why
        # the file name contains the date and time.
        gclient_utils.CheckCall(['svn', 'update', full_path],
                                print_error=False)
        gclient_utils.FileWrite(full_path, options.diff, 'wb')
      else:
        # Add the file to the repo.
        gclient_utils.FileWrite(full_path, options.diff, 'wb')
        gclient_utils.CheckCall(["svn", "add", full_path], print_error=False)
      temp_file.write(description)
      temp_file.flush()
      gclient_utils.CheckCall(["svn", "commit", full_path, '--file',
                               temp_file.name], print_error=False)
    except gclient_utils.CheckCallError, e:
      raise NoTryServerAccess(' '.join(e.command) + '\nOuput:\n' +
                              e.stdout)
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
  __pychecker__ = 'no-returnvalues'
  # Subversion has a .svn in all working directories.
  if os.path.isdir('.svn'):
    logging.info("Guessed VCS = Subversion")
    return SVN(options)

  # Git has a command to test if you're in a git tree.
  # Try running it, but don't die if we don't have git installed.
  try:
    gclient_utils.CheckCall(["git", "rev-parse", "--is-inside-work-tree"])
    logging.info("Guessed VCS = Git")
    return GIT(options)
  except gclient_utils.CheckCallError, e:
    if e.retcode != 2:  # ENOENT -- they don't have git installed.
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

  group.add_option("--project",
                   help="Override which project to use")

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
                        "patch created in a subdirectory")
  group.add_option("--patchlevel", type='int', metavar="LEVEL",
                   help="Used as -pN parameter to patch")
  parser.add_option_group(group)

  group = optparse.OptionGroup(parser, "Access the try server by HTTP")
  group.add_option("--use_http",
                   action="store_const",
                   const=_SendChangeHTTP,
                   dest="send_patch",
                   help="Use HTTP to talk to the try server [default]")
  group.add_option("--host",
                   help="Host address")
  group.add_option("--port",
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
    else:
      parser.error('Please specify an access method.')

  if len(args) == 1 and args[0] == 'help':
    parser.print_help()
  if (not options.files and (not options.issue and options.patchset) and
      not options.diff and not options.url):
    # TODO(maruel): It should just try the modified files showing up in a
    # svn status.
    parser.error('Nothing to try, changelist is empty.')

  try:
    # Convert options.diff into the content of the diff.
    if options.url:
      options.diff = urllib.urlopen(options.url).read()
    elif options.diff:
      options.diff = gclient_utils.FileRead(options.diff, 'rb')
    # Process the VCS in any case at least to retrieve the email address.
    try:
      options.scm = GuessVCS(options)
    except NoTryServerAccess, e:
      # If we got the diff, we don't care.
      if not options.diff:
        # TODO(maruel): Raise what?
        raise

    # Get try slaves from PRESUBMIT.py files if not specified.
    if not options.bot:
      # Even if the diff comes from options.url, use the local checkout for bot
      # selection.
      try:
        # Get try slaves from PRESUBMIT.py files if not specified.
        import presubmit_support
        root_presubmit = options.scm.GetBots()
        options.bot = presubmit_support.DoGetTrySlaves(
            options.scm.GetFileNames(),
            options.scm.GetLocalRoot(),
            root_presubmit,
            False,
            sys.stdout)
      except ImportError:
        pass
      # If no bot is specified, either the default pool will be selected or the
      # try server will refuse the job. Either case we don't need to interfere.

    if options.name is None:
      if options.issue:
        options.name = 'Issue %s' % options.issue
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
  sys.exit(TryChange(None, [], False))
