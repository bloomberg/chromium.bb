#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Client-side script to send a try job to the try server. It communicates to
the try server by either writting to a svn repository or by directly connecting
to the server by HTTP.
"""

import datetime
import errno
import getpass
import logging
import optparse
import os
import posixpath
import re
import shutil
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
USAGE = r"""%prog [options]

Client-side script to send a try job to the try server. It communicates to
the try server by either writting to a svn repository or by directly connecting
to the server by HTTP.

Examples:
  Try a change against a particular revision:
    %prog -r 123

  A git patch off a web site (git inserts a/ and b/) and fix the base dir:
    %prog --url http://url/to/patch.diff --patchlevel 1 --root src

  Or from rietveld:
    %prog -R codereview.chromium.org/1337 --email me@example.com --root src

  Use svn to store the try job, specify an alternate email address and use a
  premade diff file on the local drive:
    %prog --email user@example.com
            --svn_repo svn://svn.chromium.org/chrome-try/try --diff foo.diff

  Running only on a 'mac' slave with revision 123 and clobber first; specify
  manually the 3 source files to use for the try job:
    %prog --bot mac --revision 123 --clobber -f src/a.cc -f src/a.h
            -f include/b.h

  When called from gcl, use the format gcl try <change_name>.
"""

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
  def __init__(self, options, path):
    items = path.split('@')
    assert len(items) <= 2
    self.checkout_root = items[0]
    items.append(None)
    self.diff_against = items[1]
    self.options = options
    self.files = self.options.files
    self.options.files = None
    self.codereview_settings = None
    self.codereview_settings_file = 'codereview.settings'

  def GetFileNames(self):
    """Return the list of files in the diff."""
    return self.files

  def GetCodeReviewSetting(self, key):
    """Returns a value for the given key for this repository.

    Uses gcl-style settings from the repository."""
    if self.codereview_settings is None:
      self.codereview_settings = {}
      settings_file = self.ReadRootFile(self.codereview_settings_file)
      if settings_file:
        for line in settings_file.splitlines():
          if not line or line.lstrip().startswith('#'):
            continue
          k, v = line.split(":", 1)
          self.codereview_settings[k.strip()] = v.strip()
    return self.codereview_settings.get(key, '')

  def GclStyleSettings(self):
    """Set default settings based on the gcl-style settings from the
    repository."""
    settings = {
      'port': self.GetCodeReviewSetting('TRYSERVER_HTTP_PORT'),
      'host': self.GetCodeReviewSetting('TRYSERVER_HTTP_HOST'),
      'svn_repo': self.GetCodeReviewSetting('TRYSERVER_SVN_URL'),
      'project': self.GetCodeReviewSetting('TRYSERVER_PROJECT'),
      'root': self.GetCodeReviewSetting('TRYSERVER_ROOT'),
      'patchlevel': self.GetCodeReviewSetting('TRYSERVER_PATCHLEVEL'),
    }
    for (k, v) in settings.iteritems():
      if v and getattr(self.options, k) is None:
        setattr(self.options, k, v)

  def GclientStyleSettings(self):
    """Find the root, assuming a gclient-style checkout."""
    if not self.options.no_gclient and not self.options.root:
      root = self.checkout_root
      gclient_root = gclient_utils.FindGclientRoot(root)
      if gclient_root:
        self.options.root = gclient_utils.PathDifference(gclient_root, root)

  def AutomagicalSettings(self):
    """Determines settings based on supported code review and checkout tools.
    """
    self.GclStyleSettings()
    self.GclientStyleSettings()

  def ReadRootFile(self, filename):
    raise NotImplementedError()


class SVN(SCM):
  """Gathers the options and diff for a subversion checkout."""
  def __init__(self, *args, **kwargs):
    SCM.__init__(self, *args, **kwargs)
    self.checkout_root = scm.SVN.GetCheckoutRoot(self.checkout_root)
    if not self.options.email:
      # Assumes the svn credential is an email address.
      self.options.email = scm.SVN.GetEmail(self.checkout_root)
    logging.info("SVN(%s)" % self.checkout_root)

  def ReadRootFile(self, filename):
    try:
      # Try to search on the subversion repository for the file.
      import gcl
      data = gcl.GetCachedFile(filename)
      logging.debug('%s:\n%s' % (filename, data))
      return data
    except ImportError:
      try:
        data = gclient_utils.FileRead(os.path.join(self.checkout_root,
                                                   filename))
        logging.debug('%s:\n%s' % (filename, data))
        return data
      except (IOError, OSError):
        logging.debug('%s:\nNone' % filename)
        return None

  def GenerateDiff(self):
    """Returns a string containing the diff for the given file list.

    The files in the list should either be absolute paths or relative to the
    given root.
    """
    if not self.files:
      previous_cwd = os.getcwd()
      os.chdir(self.checkout_root)

      excluded = ['!', '?', 'X', ' ', '~']
      def Excluded(f):
        if f[0][0] in excluded:
          return True
        for r in self.options.exclude:
          if re.search(r, f[1]):
            logging.info('Ignoring "%s"' % f[1])
            return True
        return False

      self.files = [f[1] for f in scm.SVN.CaptureStatus(self.checkout_root)
                    if not Excluded(f)]
      os.chdir(previous_cwd)
    return scm.SVN.GenerateDiff(self.files, self.checkout_root, full_move=True,
                                revision=self.diff_against)


class GIT(SCM):
  """Gathers the options and diff for a git checkout."""
  def __init__(self, *args, **kwargs):
    SCM.__init__(self, *args, **kwargs)
    self.checkout_root = scm.GIT.GetCheckoutRoot(self.checkout_root)
    if not self.options.name:
      self.options.name = scm.GIT.GetPatchName(self.checkout_root)
    if not self.options.email:
      self.options.email = scm.GIT.GetEmail(self.checkout_root)
    logging.info("GIT(%s)" % self.checkout_root)

  def ReadRootFile(self, filename):
    try:
      # A git checkout is always a full checkout.
      data = gclient_utils.FileRead(os.path.join(self.checkout_root, filename))
      logging.debug('%s:\n%s' % (filename, data))
      return data
    except (IOError, OSError):
      logging.debug('%s:\nNone' % filename)
      return None

  def GenerateDiff(self):
    if not self.files:
      self.files = scm.GIT.GetDifferentFiles(self.checkout_root,
                                             branch=self.diff_against)

    def NotExcluded(f):
      for r in self.options.exclude:
        if re.search(r, f):
          logging.info('Ignoring "%s"' % f)
          return False
      return True

    self.files = filter(NotExcluded, self.files)
    return scm.GIT.GenerateDiff(self.checkout_root, files=self.files,
                                full_move=True,
                                branch=self.diff_against)


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
  description = ''.join("%s=%s\n" % (k,v) for (k,v) in values.iteritems())
  values['patch'] = options.diff

  url = 'http://%s:%s/send_try_patch' % (options.host, options.port)
  proxies = None
  if options.proxy:
    if options.proxy.lower() == 'none':
      # Effectively disable HTTP_PROXY or Internet settings proxy setup.
      proxies = {}
    else:
      proxies = {'http': options.proxy, 'https': options.proxy}

  logging.info('Sending by HTTP')
  logging.info(description)
  logging.info(url)
  logging.info(options.diff)
  if options.dry_run:
    return

  try:
    connection = urllib.urlopen(url, urllib.urlencode(values), proxies=proxies)
  except IOError, e:
    logging.warning(str(e))
    if (values.get('bot') and len(e.args) > 2 and
        e.args[2] == 'got a bad status line'):
      raise NoTryServerAccess('%s is unaccessible. Bad --bot argument?' % url)
    else:
      raise NoTryServerAccess('%s is unaccessible. Reason: %s' % (url,
                                                                  str(e.args)))
  if not connection:
    raise NoTryServerAccess('%s is unaccessible.' % url)
  response = connection.read()
  if response != 'OK':
    raise NoTryServerAccess('%s is unaccessible. Got:\n%s' % (url, response))


def _SendChangeSVN(options):
  """Send a change to the try server by committing a diff file on a subversion
  server."""
  if not options.svn_repo:
    raise NoTryServerAccess('Please use the --svn_repo option to specify the'
                            ' try server svn repository to connect to.')

  values = _ParseSendChangeOptions(options)
  description = ''.join("%s=%s\n" % (k,v) for (k,v) in values.iteritems())
  logging.info('Sending by SVN')
  logging.info(description)
  logging.info(options.svn_repo)
  logging.info(options.diff)
  if options.dry_run:
    return

  # Do an empty checkout.
  temp_dir = tempfile.mkdtemp()
  temp_file = tempfile.NamedTemporaryFile()
  try:
    try:
      command = ['svn', 'checkout', '--depth', 'empty', '-q',
                 options.svn_repo, temp_dir]
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
                              e.stdout + e.stderr)
  finally:
    temp_file.close()
    shutil.rmtree(temp_dir, True)


def PrintSuccess(options):
  if not options.dry_run:
    text = 'Patch \'%s\' sent to try server' % options.name
    if options.bot:
      text += ': %s' % ', '.join(options.bot)
    print(text)


def GuessVCS(options, path):
  """Helper to guess the version control system.

  NOTE: Very similar to upload.GuessVCS. Doesn't look for hg since we don't
  support it yet.

  This examines the path directory, guesses which SCM we're using, and
  returns an instance of the appropriate class.  Exit with an error if we can't
  figure it out.

  Returns:
    A SCM instance. Exits if the SCM can't be guessed.
  """
  __pychecker__ = 'no-returnvalues'
  real_path = path.split('@')[0]
  logging.info("GuessVCS(%s)" % path)
  # Subversion has a .svn in all working directories.
  if os.path.isdir(os.path.join(real_path, '.svn')):
    return SVN(options, path)

  # Git has a command to test if you're in a git tree.
  # Try running it, but don't die if we don't have git installed.
  try:
    gclient_utils.CheckCall(["git", "rev-parse", "--is-inside-work-tree"],
                            real_path)
    return GIT(options, path)
  except gclient_utils.CheckCallError, e:
    if e.retcode != errno.ENOENT and e.retcode != 128:
      # ENOENT == 2 = they don't have git installed.
      # 128 = git error code when not in a repo.
      logging.warn(e.retcode)
      raise
  raise NoTryServerAccess("Could not guess version control system. "
                          "Are you in a working copy directory?")


def GetMungedDiff(path_diff, diff):
  # Munge paths to match svn.
  for i in range(len(diff)):
    if diff[i].startswith('--- ') or diff[i].startswith('+++ '):
      new_file = posixpath.join(path_diff, diff[i][4:]).replace('\\', '/')
      diff[i] = diff[i][0:4] + new_file
  return diff


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
  parser.add_option("-v", "--verbose", action="count", default=0,
                    help="Prints debugging infos")
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
                   help="Update rietveld issue try job status. This is "
                        "optional if --issue is used, In that case, the "
                        "latest patchset will be used.")
  group.add_option("--dry_run", action='store_true',
                   help="Just prints the diff and quits")
  group.add_option("-R", "--rietveld_url", default="codereview.appspot.com",
                   metavar="URL",
                   help="The root code review url. Default:%default")
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
                   help="Override which project to use. Projects are defined "
                        "server-side to define what default bot set to use")

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
  group.add_option("-p", "--patchlevel", type='int', metavar="LEVEL",
                   help="Used as -pN parameter to patch")
  group.add_option("-s", "--sub_rep", action="append", default=[],
                   help="Subcheckout to use in addition. This is mainly "
                        "useful for gclient-style checkouts. Use @rev or "
                        "@branch or @branch1..branch2 to specify the "
                        "revision/branch to diff against.")
  # Mostly chromium-specific
  try:
    group.add_option("--webkit", action="append_const",
                     const="third_party/WebKit",
                     dest="PATH",
                     help="Shorthand for -s third_party/WebKit")
  except optparse.OptionError:
    # append_const is not supported on 2.4. Too bad.
    pass
  group.add_option("--no_gclient", action="store_true",
                   help="Disable automatic search for gclient checkout.")
  group.add_option("-E", "--exclude", action="append",
                   default=['ChangeLog'], metavar='REGEXP',
                   help="Regexp patterns to exclude files. Default: %default")
  parser.add_option_group(group)

  group = optparse.OptionGroup(parser, "Access the try server by HTTP")
  group.add_option("--use_http",
                   action="store_const",
                   const=_SendChangeHTTP,
                   dest="send_patch",
                   help="Use HTTP to talk to the try server [default]")
  group.add_option("-H", "--host",
                   help="Host address")
  group.add_option("-P", "--port",
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
  group.add_option("-S", "--svn_repo",
                   metavar="SVN_URL",
                   help="SVN url to use to write the changes in; --use_svn is "
                        "implied when using --svn_repo")
  parser.add_option_group(group)

  options, args = parser.parse_args(argv)
  if len(args) == 1 and args[0] == 'help':
    parser.print_help()

  if not swallow_exception:
    if options.verbose == 0:
      logging.basicConfig(level=logging.ERROR)
    elif options.verbose == 1:
      logging.basicConfig(level=logging.WARNING)
    elif options.verbose == 2:
      logging.basicConfig(level=logging.INFO)
    elif options.verbose > 2:
      logging.basicConfig(level=logging.DEBUG)

  logging.debug(argv)

  if options.rietveld_url:
    # Try to extract the review number if possible and fix the protocol.
    if not '://' in options.rietveld_url:
      options.rietveld_url = 'http://' + options.rietveld_url
    match = re.match(r'^(.*)/(\d+)$', options.rietveld_url)
    if match:
      if options.issue or options.patchset:
        parser.error('Cannot use both --issue and use a review number url')
      options.issue = int(match.group(2))
      options.rietveld_url = match.group(1)

  try:
    # Always include os.getcwd() in the checkout settings.
    checkouts = []
    checkouts.append(GuessVCS(options, os.getcwd()))
    checkouts[0].AutomagicalSettings()
    for item in options.sub_rep:
      checkout = GuessVCS(options, os.path.join(checkouts[0].checkout_root,
                                                item))
      if checkout.checkout_root in [c.checkout_root for c in checkouts]:
        parser.error('Specified the root %s two times.' %
                     checkout.checkout_root)
      checkouts.append(checkout)

    can_http = options.port and options.host
    can_svn = options.svn_repo
    # If there was no transport selected yet, now we must have enough data to
    # select one.
    if not options.send_patch and not (can_http or can_svn):
      parser.error('Please specify an access method.')

    # Convert options.diff into the content of the diff.
    if options.url:
      if options.files:
        parser.error('You cannot specify files and --url at the same time.')
      options.diff = urllib.urlopen(options.url).read()
    elif options.diff:
      if options.files:
        parser.error('You cannot specify files and --diff at the same time.')
      options.diff = gclient_utils.FileRead(options.diff, 'rb')
    elif options.issue and options.patchset is None:
      # Retrieve the patch from rietveld when the diff is not specified.
      # When patchset is specified, it's because it's done by gcl/git-try.
      try:
        import simplejson
      except ImportError:
        parser.error('simplejson library is missing, please install.')
      api_url = '%s/api/%d' % (options.rietveld_url, options.issue)
      logging.debug(api_url)
      contents = simplejson.loads(urllib.urlopen(api_url).read())
      options.patchset = contents['patchsets'][-1]
      diff_url = ('%s/download/issue%d_%d.diff' %
          (options.rietveld_url,  options.issue, options.patchset))
      diff = GetMungedDiff('', urllib.urlopen(diff_url).readlines())
      options.diff = ''.join(diff)
    else:
      # Use this as the base.
      root = checkouts[0].checkout_root
      diffs = []
      for checkout in checkouts:
        diff = checkout.GenerateDiff().splitlines(True)
        path_diff = gclient_utils.PathDifference(root, checkout.checkout_root)
        # Munge it.
        diffs.extend(GetMungedDiff(path_diff, diff))
      options.diff = ''.join(diffs)

    if not options.bot:
      # Get try slaves from PRESUBMIT.py files if not specified.
      # Even if the diff comes from options.url, use the local checkout for bot
      # selection.
      try:
        import presubmit_support
        root_presubmit = checkouts[0].ReadRootFile('PRESUBMIT.py')
        options.bot = presubmit_support.DoGetTrySlaves(
            checkouts[0].GetFileNames(),
            checkouts[0].checkout_root,
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
      parser.error('Using an anonymous checkout. Please use --email or set '
                   'the TRYBOT_RESULTS_EMAIL_ADDRESS environment variable.')
    else:
      print('Results will be emailed to: ' + options.email)

    # Send the patch.
    if options.send_patch:
      # If forced.
      options.send_patch(options)
      PrintSuccess(options)
      return 0
    try:
      if can_http:
        _SendChangeHTTP(options)
        PrintSuccess(options)
        return 0
    except NoTryServerAccess:
      if not can_svn:
        raise
    _SendChangeSVN(options)
    PrintSuccess(options)
    return 0
  except (InvalidScript, NoTryServerAccess), e:
    if swallow_exception:
      return 1
    print e
    return 1
  return 0


if __name__ == "__main__":
  sys.exit(TryChange(None, [], False))
