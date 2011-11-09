#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  import simplejson as json  # pylint: disable=F0401
except ImportError:
  try:
    import json  # pylint: disable=F0401
  except ImportError:
    # Import the one included in depot_tools.
    sys.path.append(os.path.join(os.path.dirname(__file__), 'third_party'))
    import simplejson as json  # pylint: disable=F0401

import breakpad  # pylint: disable=W0611

import gcl
import fix_encoding
import gclient_utils
import scm
import subprocess2


__version__ = '1.2'


# Constants
HELP_STRING = "Sorry, Tryserver is not available."
USAGE = r"""%prog [options]

Client-side script to send a try job to the try server. It communicates to
the try server by either writting to a svn repository or by directly connecting
to the server by HTTP."""

EPILOG = """
Examples:
  Send a patch directly from rietveld:
    %(prog)s -R codereview.chromium.org/1337
        --email recipient@example.com --root src

  Try a change against a particular revision:
    %(prog)s -r 123

  A git patch off a web site (git inserts a/ and b/) and fix the base dir:
    %(prog)s --url http://url/to/patch.diff --patchlevel 1 --root src

  Use svn to store the try job, specify an alternate email address and use a
  premade diff file on the local drive:
    %(prog)s --email user@example.com
            --svn_repo svn://svn.chromium.org/chrome-try/try --diff foo.diff

  Running only on a 'mac' slave with revision 123 and clobber first; specify
  manually the 3 source files to use for the try job:
    %(prog)s --bot mac --revision 123 --clobber -f src/a.cc -f src/a.h
            -f include/b.h
"""

class InvalidScript(Exception):
  def __str__(self):
    return self.args[0] + '\n' + HELP_STRING


class NoTryServerAccess(Exception):
  def __str__(self):
    return self.args[0] + '\n' + HELP_STRING


def Escape(name):
  """Escapes characters that could interfere with the file system or try job
  parsing.
  """
  return re.sub(r'[^\w#-]', '_', name)


class SCM(object):
  """Simplistic base class to implement one function: ProcessOptions."""
  def __init__(self, options, path, file_list):
    items = path.split('@')
    assert len(items) <= 2
    self.checkout_root = os.path.abspath(items[0])
    items.append(None)
    self.diff_against = items[1]
    self.options = options
    # Lazy-load file list from the SCM unless files were specified in options.
    self._files = None
    self._file_tuples = None
    if file_list:
      self._files = file_list
      self._file_tuples = [('M', f) for f in self.files]
    self.options.files = None
    self.codereview_settings = None
    self.codereview_settings_file = 'codereview.settings'
    self.toplevel_root = None

  def GetFileNames(self):
    """Return the list of files in the diff."""
    return self.files

  def GetCodeReviewSetting(self, key):
    """Returns a value for the given key for this repository.

    Uses gcl-style settings from the repository.
    """
    if gcl:
      gcl_setting = gcl.GetCodeReviewSetting(key)
      if gcl_setting != '':
        return gcl_setting
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

  def _GclStyleSettings(self):
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
    logging.info('\n'.join(['%s: %s' % (k, v)
                            for (k, v) in settings.iteritems() if v]))
    for (k, v) in settings.iteritems():
      if v and getattr(self.options, k) is None:
        setattr(self.options, k, v)

  def AutomagicalSettings(self):
    """Determines settings based on supported code review and checkout tools.
    """
    # Try to find gclient or repo root first.
    if not self.options.no_search:
      self.toplevel_root = gclient_utils.FindGclientRoot(self.checkout_root)
      if self.toplevel_root:
        logging.info('Found .gclient at %s' % self.toplevel_root)
      else:
        self.toplevel_root = gclient_utils.FindFileUpwards(
            os.path.join('..', '.repo'), self.checkout_root)
        if self.toplevel_root:
          logging.info('Found .repo dir at %s'
                       % os.path.dirname(self.toplevel_root))

      if self.toplevel_root and not self.options.root:
        assert os.path.abspath(self.toplevel_root) == self.toplevel_root
        self.options.root = gclient_utils.PathDifference(self.toplevel_root,
                                                         self.checkout_root)

    self._GclStyleSettings()

  def ReadRootFile(self, filename):
    cur = self.checkout_root
    root = self.toplevel_root or self.checkout_root

    assert cur.startswith(root), (root, cur)
    while cur.startswith(root):
      filepath = os.path.join(cur, filename)
      if os.path.isfile(filepath):
        logging.info('Found %s at %s' % (filename, cur))
        return gclient_utils.FileRead(filepath)
      cur = os.path.dirname(cur)
    logging.warning('Didn\'t find %s' % filename)
    return None

  def _SetFileTuples(self, file_tuples):
    excluded = ['!', '?', 'X', ' ', '~']
    def Excluded(f):
      if f[0][0] in excluded:
        return True
      for r in self.options.exclude:
        if re.search(r, f[1]):
          logging.info('Ignoring "%s"' % f[1])
          return True
      return False

    self._file_tuples = [f for f in file_tuples if not Excluded(f)]
    self._files = [f[1] for f in self._file_tuples]

  def CaptureStatus(self):
    """Returns the 'svn status' emulated output as an array of (status, file)
       tuples."""
    raise NotImplementedError(
        "abstract method -- subclass %s must override" % self.__class__)

  @property
  def files(self):
    if self._files is None:
      self._SetFileTuples(self.CaptureStatus())
    return self._files

  @property
  def file_tuples(self):
    if self._file_tuples is None:
      self._SetFileTuples(self.CaptureStatus())
    return self._file_tuples


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
    data = SCM.ReadRootFile(self, filename)
    if data:
      return data

    # Try to search on the subversion repository for the file.
    if not gcl:
      return None
    data = gcl.GetCachedFile(filename)
    logging.debug('%s:\n%s' % (filename, data))
    return data

  def CaptureStatus(self):
    previous_cwd = os.getcwd()
    os.chdir(self.checkout_root)
    result = scm.SVN.CaptureStatus(self.checkout_root)
    os.chdir(previous_cwd)
    return result

  def GenerateDiff(self):
    """Returns a string containing the diff for the given file list.

    The files in the list should either be absolute paths or relative to the
    given root.
    """
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
    if not self.diff_against:
      self.diff_against = scm.GIT.GetUpstreamBranch(self.checkout_root)
      if not self.diff_against:
        raise NoTryServerAccess(
            "Unable to determine default branch to diff against. "
            "Verify this branch is set up to track another"
            "(via the --track argument to \"git checkout -b ...\"")
    logging.info("GIT(%s)" % self.checkout_root)

  def CaptureStatus(self):
    return scm.GIT.CaptureStatus(self.checkout_root.replace(os.sep, '/'),
                                 self.diff_against)

  def GenerateDiff(self):
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
  if options.testfilter:
    values['testfilter'] = ','.join(options.testfilter)
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
  description = ''.join("%s=%s\n" % (k, v) for (k, v) in values.iteritems())
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
    logging.info('Opening connection...')
    connection = urllib.urlopen(url, urllib.urlencode(values), proxies=proxies)
    logging.info('Done')
  except IOError, e:
    logging.info(str(e))
    if (values.get('bot') and len(e.args) > 2 and
        e.args[2] == 'got a bad status line'):
      raise NoTryServerAccess('%s is unaccessible. Bad --bot argument?' % url)
    else:
      raise NoTryServerAccess('%s is unaccessible. Reason: %s' % (url,
                                                                  str(e.args)))
  if not connection:
    raise NoTryServerAccess('%s is unaccessible.' % url)
  logging.info('Reading response...')
  response = connection.read()
  logging.info('Done')
  if response != 'OK':
    raise NoTryServerAccess('%s is unaccessible. Got:\n%s' % (url, response))


def _SendChangeSVN(options):
  """Send a change to the try server by committing a diff file on a subversion
  server."""
  if not options.svn_repo:
    raise NoTryServerAccess('Please use the --svn_repo option to specify the'
                            ' try server svn repository to connect to.')

  values = _ParseSendChangeOptions(options)
  description = ''.join("%s=%s\n" % (k, v) for (k, v) in values.iteritems())
  logging.info('Sending by SVN')
  logging.info(description)
  logging.info(options.svn_repo)
  logging.info(options.diff)
  if options.dry_run:
    return

  # Create a temporary directory, put a uniquely named file in it with the diff
  # content and svn import that.
  temp_dir = tempfile.mkdtemp()
  temp_file = tempfile.NamedTemporaryFile()
  try:
    try:
      # Description
      temp_file.write(description)
      temp_file.flush()

      # Diff file
      current_time = str(datetime.datetime.now()).replace(':', '.')
      file_name = (Escape(options.user) + '.' + Escape(options.name) +
                   '.%s.diff' % current_time)
      full_path = os.path.join(temp_dir, file_name)
      gclient_utils.FileWrite(full_path, options.diff, 'wb')

      # Committing it will trigger a try job.
      if sys.platform == "cygwin":
        # Small chromium-specific issue here:
        # git-try uses /usr/bin/python on cygwin but svn.bat will be used
        # instead of /usr/bin/svn by default. That causes bad things(tm) since
        # Windows' svn.exe has no clue about cygwin paths. Hence force to use
        # the cygwin version in this particular context.
        exe = "/usr/bin/svn"
      else:
        exe = "svn"
      command = [exe, 'import', '-q', temp_dir, options.svn_repo, '--file',
                 temp_file.name]
      if scm.SVN.AssertVersion("1.5")[0]:
        command.append('--no-ignore')

      subprocess2.check_call(command)
    except subprocess2.CalledProcessError, e:
      raise NoTryServerAccess(str(e))
  finally:
    temp_file.close()
    shutil.rmtree(temp_dir, True)


def PrintSuccess(options):
  if not options.dry_run:
    text = 'Patch \'%s\' sent to try server' % options.name
    if options.bot:
      text += ': %s' % ', '.join(options.bot)
    print(text)


def GuessVCS(options, path, file_list):
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
    return SVN(options, path, file_list)

  # Git has a command to test if you're in a git tree.
  # Try running it, but don't die if we don't have git installed.
  try:
    subprocess2.check_output(
        ['git', 'rev-parse', '--is-inside-work-tree'], cwd=real_path,
        stderr=subprocess2.VOID)
    return GIT(options, path, file_list)
  except OSError, e:
    if e.errno != errno.ENOENT:
      raise
  except subprocess2.CalledProcessError, e:
    if e.returncode != errno.ENOENT and e.returncode != 128:
      # ENOENT == 2 = they don't have git installed.
      # 128 = git error code when not in a repo.
      logging.warning('Unexpected error code: %s' % e.returncode)
      raise
  raise NoTryServerAccess("Could not guess version control system. "
                          "Are you in a working copy directory?")


def GetMungedDiff(path_diff, diff):
  # Munge paths to match svn.
  changed_files = []
  for i in range(len(diff)):
    if diff[i].startswith('--- ') or diff[i].startswith('+++ '):
      new_file = posixpath.join(path_diff, diff[i][4:]).replace('\\', '/')
      if diff[i].startswith('--- '):
        file_path = new_file.split('\t')[0].strip()
        if file_path.startswith('a/'):
          file_path = file_path[2:]
        changed_files.append(('M', file_path))
      diff[i] = diff[i][0:4] + new_file
  return (diff, changed_files)


def TryChange(argv,
              change,
              swallow_exception,
              prog=None,
              extra_epilog=None):
  """
  Args:
    argv: Arguments and options.
    change: Change instance corresponding to the CL.
    swallow_exception: Whether we raise or swallow exceptions.
  """
  # Parse argv
  parser = optparse.OptionParser(usage=USAGE,
                                 version=__version__,
                                 prog=prog)
  epilog = EPILOG % { 'prog': prog }
  if extra_epilog:
    epilog += extra_epilog
  parser.epilog = epilog
  # Remove epilog formatting
  parser.format_epilog = lambda x: parser.epilog
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
                   help="Don't send the try job. This implies --verbose, so "
                        "it will print the diff.")
  parser.add_option_group(group)

  group = optparse.OptionGroup(parser, "Try job options")
  group.add_option("-b", "--bot", action="append",
                    help="Only use specifics build slaves, ex: "
                         "'--bot win,layout_mac'; see the try "
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

  group.add_option("-t", "--testfilter", action="append",
                   help="Add a gtest_filter to a test. Use multiple times to "
                        "specify filters for different tests. (i.e. "
                        "--testfilter base_unittests:ThreadTest.* "
                        "--testfilter ui_tests) If you specify any testfilters "
                        "the test results will not be reported in rietveld and "
                        "only tests with filters will run.")

  parser.add_option_group(group)

  group = optparse.OptionGroup(parser, "Patch to run")
  group.add_option("-f", "--file", default=[], dest="files",
                   metavar="FILE", action="append",
                   help="Use many times to list the files to include in the "
                        "try, relative to the repository root")
  group.add_option("--diff",
                   help="File containing the diff to try")
  group.add_option("--url",
                   help="Url where to grab a patch, e.g. "
                        "http://example.com/x.diff")
  group.add_option("-R", "--rietveld_url", default="codereview.appspot.com",
                   metavar="URL",
                   help="Has 2 usages, both refer to the rietveld instance: "
                        "Specify which code review patch to use as the try job "
                        "or rietveld instance to update the try job results "
                        "Default:%default")
  group.add_option("--root",
                   help="Root to use for the patch; base subdirectory for "
                        "patch created in a subdirectory")
  group.add_option("-p", "--patchlevel", type='int', metavar="LEVEL",
                   help="Used as -pN parameter to patch")
  group.add_option("-s", "--sub_rep", action="append", default=[],
                   help="Subcheckout to use in addition. This is mainly "
                        "useful for gclient-style checkouts. In git, checkout "
                        "the branch with changes first. Use @rev or "
                        "@branch to specify the "
                        "revision/branch to diff against. If no @branch is "
                        "given the diff will be against the upstream branch. "
                        "If @branch then the diff is branch..HEAD. "
                        "All edits must be checked in.")
  group.add_option("--no_search", action="store_true",
                   help=("Disable automatic search for gclient or repo "
                        "checkout root."))
  group.add_option("-E", "--exclude", action="append",
                   default=['ChangeLog'], metavar='REGEXP',
                   help="Regexp patterns to exclude files. Default: %default")
  group.add_option("--upstream_branch", action="store",
                   help="Specify the upstream branch to diff against in the "
                        "main checkout")
  parser.add_option_group(group)

  group = optparse.OptionGroup(parser, "Access the try server by HTTP")
  group.add_option("--use_http",
                   action="store_const",
                   const=_SendChangeHTTP,
                   dest="send_patch",
                   help="Use HTTP to talk to the try server [default]")
  group.add_option("-H", "--host",
                   help="Host address")
  group.add_option("-P", "--port", type="int",
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

  # If they've asked for help, give it to them
  if len(args) == 1 and args[0] == 'help':
    parser.print_help()
    return 0

  # If they've said something confusing, don't spawn a try job until you
  # understand what they want.
  if args:
    parser.error('Extra argument(s) "%s" not understood' % ' '.join(args))

  if options.dry_run:
    options.verbose += 1

  LOG_FORMAT = '%(levelname)s %(filename)s(%(lineno)d): %(message)s'
  if not swallow_exception:
    if options.verbose == 0:
      logging.basicConfig(level=logging.WARNING, format=LOG_FORMAT)
    elif options.verbose == 1:
      logging.basicConfig(level=logging.INFO, format=LOG_FORMAT)
    elif options.verbose > 1:
      logging.basicConfig(level=logging.DEBUG, format=LOG_FORMAT)

  logging.debug(argv)

  if (options.patchlevel is not None and
      (options.patchlevel < 0 or options.patchlevel > 10)):
    parser.error(
        'Have you tried --port instead? You probably confused -p and -P.')

  # Strip off any @ in the user, otherwise svn gets confused.
  options.user = options.user.split('@', 1)[0]

  if options.rietveld_url:
    # Try to extract the review number if possible and fix the protocol.
    if not '://' in options.rietveld_url:
      options.rietveld_url = 'http://' + options.rietveld_url
    match = re.match(r'^(.*)/(\d+)/?$', options.rietveld_url)
    if match:
      if options.issue or options.patchset:
        parser.error('Cannot use both --issue and use a review number url')
      options.issue = int(match.group(2))
      options.rietveld_url = match.group(1)

  try:
    changed_files = None
    # Always include os.getcwd() in the checkout settings.
    checkouts = []
    path = os.getcwd()

    file_list = []
    if options.files:
      file_list = options.files
    elif change:
      file_list = [f.LocalPath() for f in change.AffectedFiles()]

    if options.upstream_branch:
      path += '@' + options.upstream_branch
      # Clear file list so that the correct list will be retrieved from the
      # upstream branch.
      file_list = []
    checkouts.append(GuessVCS(options, path, file_list))
    checkouts[0].AutomagicalSettings()
    for item in options.sub_rep:
      checkout = GuessVCS(options, os.path.join(checkouts[0].checkout_root,
                                                item), file_list)
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
      if json is None:
        parser.error('json or simplejson library is missing, please install.')
      api_url = '%s/api/%d' % (options.rietveld_url, options.issue)
      logging.debug(api_url)
      contents = json.loads(urllib.urlopen(api_url).read())
      options.patchset = contents['patchsets'][-1]
      diff_url = ('%s/download/issue%d_%d.diff' %
          (options.rietveld_url,  options.issue, options.patchset))
      diff = GetMungedDiff('', urllib.urlopen(diff_url).readlines())
      options.diff = ''.join(diff[0])
      changed_files = diff[1]
    else:
      # Use this as the base.
      root = checkouts[0].checkout_root
      diffs = []
      for checkout in checkouts:
        diff = checkout.GenerateDiff().splitlines(True)
        path_diff = gclient_utils.PathDifference(root, checkout.checkout_root)
        # Munge it.
        diffs.extend(GetMungedDiff(path_diff, diff)[0])
      options.diff = ''.join(diffs)

    if not options.name:
      if options.issue:
        options.name = 'Issue %s' % options.issue
      else:
        options.name = 'Unnamed'
        print('Note: use --name NAME to change the try job name.')

    if not options.email:
      parser.error('Using an anonymous checkout. Please use --email or set '
                   'the TRYBOT_RESULTS_EMAIL_ADDRESS environment variable.')
    print('Results will be emailed to: ' + options.email)

    if not options.bot:
      # Get try slaves from PRESUBMIT.py files if not specified.
      # Even if the diff comes from options.url, use the local checkout for bot
      # selection.
      try:
        import presubmit_support
        root_presubmit = checkouts[0].ReadRootFile('PRESUBMIT.py')
        if not change:
          if not changed_files:
            changed_files = checkouts[0].file_tuples
          change = presubmit_support.Change(options.name,
                                            '',
                                            checkouts[0].checkout_root,
                                            changed_files,
                                            options.issue,
                                            options.patchset,
                                            options.email)
        options.bot = presubmit_support.DoGetTrySlaves(
            change,
            checkouts[0].GetFileNames(),
            checkouts[0].checkout_root,
            root_presubmit,
            options.project,
            False,
            sys.stdout)
      except ImportError:
        pass
      # If no bot is specified, either the default pool will be selected or the
      # try server will refuse the job. Either case we don't need to interfere.

    # Prevent rietveld updates if we aren't running all the tests.
    if options.testfilter is not None:
      options.issue = None
      options.patchset = None

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
    print >> sys.stderr, e
    return 1
  except (gclient_utils.Error, subprocess2.CalledProcessError), e:
    print >> sys.stderr, e
    return 1
  return 0


if __name__ == "__main__":
  fix_encoding.fix_encoding()
  sys.exit(TryChange(None, None, False))
