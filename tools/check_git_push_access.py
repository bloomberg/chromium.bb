#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that attempts to push to a special git repository to verify that git
credentials are configured correctly. It also attempts to fix misconfigurations
if possible.

It will be added as gclient hook shortly before Chromium switches to git and
removed after the switch.

When running as hook in *.corp.google.com network it will also report status
of the push attempt to the server (on appengine), so that chrome-infra team can
collect information about misconfigured Git accounts (to fix them).

When invoked manually will do the access test and submit the report regardless
of where it is running.
"""

import contextlib
import getpass
import json
import logging
import netrc
import optparse
import os
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
import time
import urllib2


# Absolute path to src/ directory.
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Incremented whenever some changes to scrip logic are made. Change in version
# will cause the check to be rerun on next gclient runhooks invocation.
CHECKER_VERSION = 0

# URL to POST json with results to.
MOTHERSHIP_URL = (
    'https://chromium-git-access.appspot.com/'
    'git_access/api/v1/reports/access_check')

# Repository to push test commits to.
TEST_REPO_URL = 'https://chromium.googlesource.com/a/playground/access_test'

# Possible chunks of git push response in case .netrc is misconfigured.
BAD_ACL_ERRORS = (
  '(prohibited by Gerrit)',
  'Invalid user name or password',
)


def is_on_bot():
  """True when running under buildbot."""
  return os.environ.get('CHROME_HEADLESS') == '1'


def is_in_google_corp():
  """True when running in google corp network."""
  try:
    return socket.getfqdn().endswith('.corp.google.com')
  except socket.error:
    logging.exception('Failed to get FQDN')
    return False


def is_using_git():
  """True if git checkout is used."""
  return os.path.exists(os.path.join(REPO_ROOT, '.git', 'objects'))


def is_using_svn():
  """True if svn checkout is used."""
  return os.path.exists(os.path.join(REPO_ROOT, '.svn'))


def read_git_config(prop):
  """Reads git config property of src.git repo."""
  proc = subprocess.Popen(
      ['git', 'config', prop], stdout=subprocess.PIPE, cwd=REPO_ROOT)
  out, _ = proc.communicate()
  return out.strip()


def read_netrc_user(netrc_obj, host):
  """Reads 'user' field of a host entry in netrc.

  Returns empty string if netrc is missing, or host is not there.
  """
  if not netrc_obj:
    return ''
  entry = netrc_obj.authenticators(host)
  if not entry:
    return ''
  return entry[0]


def get_git_version():
  """Returns version of git or None if git is not available."""
  proc = subprocess.Popen(['git', '--version'], stdout=subprocess.PIPE)
  out, _ = proc.communicate()
  return out.strip() if proc.returncode == 0 else ''


def scan_configuration():
  """Scans local environment for git related configuration values."""
  # Git checkout?
  is_git = is_using_git()

  # On Windows HOME should be set.
  if 'HOME' in os.environ:
    netrc_path = os.path.join(
        os.environ['HOME'],
        '_netrc' if sys.platform.startswith('win') else '.netrc')
  else:
    netrc_path = None

  # Netrc exists?
  is_using_netrc = netrc_path and os.path.exists(netrc_path)

  # Read it.
  netrc_obj = None
  if is_using_netrc:
    try:
      netrc_obj = netrc.netrc(netrc_path)
    except Exception:
      logging.exception('Failed to read netrc from %s', netrc_path)
      netrc_obj = None

  return {
    'checker_version': CHECKER_VERSION,
    'is_git': is_git,
    'is_home_set': 'HOME' in os.environ,
    'is_using_netrc': is_using_netrc,
    'netrc_file_mode': os.stat(netrc_path).st_mode if is_using_netrc else 0,
    'git_version': get_git_version(),
    'platform': sys.platform,
    'username': getpass.getuser(),
    'git_user_email': read_git_config('user.email') if is_git else '',
    'git_user_name': read_git_config('user.name') if is_git else '',
    'chromium_netrc_email':
        read_netrc_user(netrc_obj, 'chromium.googlesource.com'),
    'chrome_internal_netrc_email':
        read_netrc_user(netrc_obj, 'chrome-internal.googlesource.com'),
  }


def last_configuration_path():
  """Path to store last checked configuration."""
  if is_using_git():
    return os.path.join(REPO_ROOT, '.git', 'check_git_access_conf.json')
  elif is_using_svn():
    return os.path.join(REPO_ROOT, '.svn', 'check_git_access_conf.json')
  else:
    return os.path.join(REPO_ROOT, '.check_git_access_conf.json')


def read_last_configuration():
  """Reads last checked configuration if it exists."""
  try:
    with open(last_configuration_path(), 'r') as f:
      return json.load(f)
  except (IOError, ValueError):
    return None


def write_last_configuration(conf):
  """Writes last checked configuration to a file."""
  try:
    with open(last_configuration_path(), 'w') as f:
      json.dump(conf, f, indent=2, sort_keys=True)
  except IOError:
    logging.exception('Failed to write JSON to %s', path)


@contextlib.contextmanager
def temp_directory():
  """Creates a temp directory, then nukes it."""
  tmp = tempfile.mkdtemp()
  try:
    yield tmp
  finally:
    try:
      shutil.rmtree(tmp)
    except (OSError, IOError):
      logging.exception('Failed to remove temp directory %s', tmp)


class Runner(object):
  """Runs a bunch of commands in some directory, collects logs from them."""

  def __init__(self, cwd):
    self.cwd = cwd
    self.log = []

  def run(self, cmd):
    log = ['> ' + ' '.join(cmd)]
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=self.cwd)
    out, _ = proc.communicate()
    out = out.strip()
    if out:
      log.append(out)
    if proc.returncode:
      log.append('(exit code: %d)' % proc.returncode)
    self.log.append('\n'.join(log))
    return proc.returncode


def check_git_access(conf, report_url, interactive):
  """Attempts to push to a git repository, reports results to a server.

  Returns True if the check finished without incidents (push itself may
  have failed) and should NOT be retried on next invocation of the hook.
  """
  logging.warning('Checking push access to the git repository...')

  # Don't even try to push if netrc is not configured.
  if not conf['chromium_netrc_email']:
    return upload_report(
        conf,
        report_url,
        interactive,
        push_works=False,
        push_log='',
        push_duration_ms=0)

  # Ref to push to, each user has its own ref.
  ref = 'refs/push-test/%s' % conf['chromium_netrc_email']

  push_works = False
  flake = False
  started = time.time()
  try:
    with temp_directory() as tmp:
      # Prepare a simple commit on a new timeline.
      runner = Runner(tmp)
      runner.run(['git', 'init', '.'])
      if conf['git_user_name']:
        runner.run(['git', 'config', 'user.name', conf['git_user_name']])
      if conf['git_user_email']:
        runner.run(['git', 'config', 'user.email', conf['git_user_email']])
      with open(os.path.join(tmp, 'timestamp'), 'w') as f:
        f.write(str(int(time.time() * 1000)))
      runner.run(['git', 'add', 'timestamp'])
      runner.run(['git', 'commit', '-m', 'Push test.'])
      # Try to push multiple times if it fails due to issues other than ACLs.
      attempt = 0
      while attempt < 5:
        attempt += 1
        logging.info('Pushing to %s %s', TEST_REPO_URL, ref)
        ret = runner.run(['git', 'push', TEST_REPO_URL, 'HEAD:%s' % ref, '-f'])
        if not ret:
          push_works = True
          break
        if any(x in runner.log[-1] for x in BAD_ACL_ERRORS):
          push_works = False
          break
  except Exception:
    logging.exception('Unexpected exception when pushing')
    flake = True

  uploaded = upload_report(
      conf,
      report_url,
      interactive,
      push_works=push_works,
      push_log='\n'.join(runner.log),
      push_duration_ms=int((time.time() - started) * 1000))
  return uploaded and not flake


def upload_report(
    conf, report_url, interactive, push_works, push_log, push_duration_ms):
  """Posts report to the server, returns True if server accepted it.

  If interactive is True and the script is running outside of *.corp.google.com
  network, will ask the user to submit collected information manually.
  """
  report = conf.copy()
  report.update(
      push_works=push_works,
      push_log=push_log,
      push_duration_ms=push_duration_ms)

  as_bytes = json.dumps({'access_check': report}, indent=2, sort_keys=True)

  if interactive:
    print 'Status of git push attempt:'
    print as_bytes

  # Do not upload it outside of corp.
  if not is_in_google_corp():
    if interactive:
      print (
          'You can send the above report to chrome-git-migration@google.com '
          'if you need help to set up you committer git account.')
    return True

  req = urllib2.Request(
      url=report_url,
      data=as_bytes,
      headers={'Content-Type': 'application/json; charset=utf-8'})

  attempt = 0
  success = False
  while not success and attempt < 10:
    attempt += 1
    try:
      logging.info('Attempting to upload the report to %s', report_url)
      urllib2.urlopen(req, timeout=5)
      success = True
      logging.warning('Report uploaded.')
    except (urllib2.URLError, socket.error, ssl.SSLError) as exc:
      logging.info('Failed to upload the report: %s', exc)
  return success


def main(args):
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option(
      '--running-as-hook',
      action='store_true',
      help='Set when invoked from gclient hook')
  parser.add_option(
      '--report-url',
      default=MOTHERSHIP_URL,
      help='URL to submit the report to')
  parser.add_option(
      '--verbose',
      action='store_true',
      help='More logging')
  options, args = parser.parse_args()
  if args:
    parser.error('Unknown argument %s' % args)
  logging.basicConfig(
      format='%(message)s',
      level=logging.INFO if options.verbose else logging.WARN)

  # When invoked not as hook, always run the check.
  if not options.running_as_hook:
    if check_git_access(scan_configuration(), options.report_url, True):
      return 0
    return 1

  # Otherwise, do it only on google owned, non-bot machines.
  if is_on_bot() or not is_in_google_corp():
    logging.info('Skipping the check: bot or non corp.')
    return 0

  # Skip the check if current configuration was already checked.
  config = scan_configuration()
  if config == read_last_configuration():
    logging.info('Check already performed, skipping.')
    return 0

  # Run the check. Mark configuration as checked only on success. Ignore any
  # exceptions or errors. This check must not break gclient runhooks.
  try:
    ok = check_git_access(config, options.report_url, False)
    if ok:
      write_last_configuration(config)
    else:
      logging.warning('Check failed and will be retried on the next run')
  except Exception:
    logging.exception('Unexpected exception when performing git access check')
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
