#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Validate or replace the standard gdata authorization token."""

import filecmp
import optparse
import os
import shutil

from chromite.buildbot import constants
from chromite.lib import cros_build_lib as build_lib
from chromite.lib import operation

MODULE = os.path.splitext(os.path.basename(__file__))[0]
oper = operation.Operation(MODULE)

TOKEN_FILE = os.path.join(os.environ['HOME'], '.gdata_token')
CRED_FILE = os.path.join(os.environ['HOME'], '.gdata_cred.txt')


def _ChrootPathToExternalPath(path):
  """Translate |path| inside chroot to external path to same location."""
  if path:
    return os.path.join(constants.SOURCE_ROOT,
                        constants.DEFAULT_CHROOT_DIR,
                        path.lstrip('/'))

  return None


class OutsideChroot(object):
  """Class for managing functionality when run outside chroot."""

  def __init__(self, args):
    self.args = args

  def Run(self):
    """Re-start |args| inside chroot and copy out auth file."""

    # Note that enter_chroot (cros_sdk) will automatically copy both
    # the token file and the cred file into the chroot, so no need
    # to do that here.

    # Rerun the same command that launched this run inside the chroot.
    cmd = [MODULE] + self.args
    result = build_lib.RunCommand(cmd, enter_chroot=True,
                                  print_cmd=False, error_code_ok=True)
    if result.returncode != 0:
      oper.Die('Token validation failed, exit code was %r.' %
               result.returncode)

    # Copy the token file back from chroot if different.
    chroot_token_file = _ChrootPathToExternalPath(TOKEN_FILE)
    if not os.path.exists(chroot_token_file):
      oper.Die('No token file generated inside chroot.')
    elif (not os.path.exists(TOKEN_FILE) or not
          filecmp.cmp(TOKEN_FILE, chroot_token_file)):
      oper.Notice('Copying new token file from chroot to %r' % TOKEN_FILE)
      shutil.copy2(chroot_token_file, TOKEN_FILE)
    else:
      oper.Notice('No change in token file.')


class InsideChroot(object):
  """Class for managing functionality when run inside chroot.

  Note that some additional imports happen within code in this class
  because those imports are only available inside the chroot.
  """

  def __init__(self):
    self.creds = None     # gdata_lib.Creds object.
    self.gd_client = None # For interacting with Google Docs.
    self.it_client = None # For interacting with Issue Tracker.

  def _LoadTokenFile(self):
    """Load existing auth token file."""
    if not os.path.exists(TOKEN_FILE):
      oper.Warning('No current token file at %r.' % TOKEN_FILE)
      return False

    # Load token file, if it exists.
    self.creds.LoadAuthToken(TOKEN_FILE)
    return True

  def _SaveTokenFile(self):
    """Save to auth toke file if anything changed."""
    self.creds.StoreAuthTokenIfNeeded(TOKEN_FILE)

  def _ValidateDocsToken(self):
    """Validate the existing Docs token."""
    # pylint: disable=W0404
    import gdata.service

    if not self.creds.docs_auth_token:
      return False

    oper.Notice('Attempting to log into Docs using auth token.')
    self.gd_client.source = 'Package Status'
    self.gd_client.SetClientLoginToken(self.creds.docs_auth_token)

    try:
      # Try to access generic spreadsheets feed, which will check access.
      self.gd_client.GetSpreadsheetsFeed()

      # Token accepted.  We're done here.
      oper.Notice('Docs token validated.')
      return True
    except gdata.service.RequestError as ex:
      reason = ex[0]['reason']
      if reason == 'Token expired':
        return False

      raise

  def _GenerateDocsToken(self):
    """Generate a new Docs token from credentials."""
    # pylint: disable=W0404
    import gdata.service

    oper.Warning('Docs token not valid.  Will try to generate a new one.')
    self.creds.LoadCreds(CRED_FILE)
    self.gd_client.email = self.creds.user
    self.gd_client.password = self.creds.password

    try:
      self.gd_client.ProgrammaticLogin()
      self.creds.SetDocsAuthToken(self.gd_client.GetClientLoginToken())

      oper.Notice('New Docs token generated.')
      return True
    except gdata.service.BadAuthentication:
      oper.Error('Credentials from %r not accepted.'
                 '  Unable to generate new Docs token.' % CRED_FILE)
      return False

  def _ValidateTrackerToken(self):
    """Validate the existing Tracker token."""
    # pylint: disable=W0404
    import gdata.gauth
    import gdata.projecthosting.client

    if not self.creds.tracker_auth_token:
      return False

    oper.Notice('Attempting to log into Tracker using auth token.')
    self.it_client.source = 'Package Status'
    self.it_client.auth_token = gdata.gauth.ClientLoginToken(
      self.creds.tracker_auth_token)

    try:
      # Try to access Tracker Issue #1, which will check access.
      query = gdata.projecthosting.client.Query(issue_id='1')
      self.it_client.get_issues('chromium-os', query=query)

      # Token accepted.  We're done here.
      oper.Notice('Tracker token validated.')
      return True
    except gdata.client.Error:
      # Exception is gdata.client.Unauthorized in the case of bad token, but
      # I do not know what the error is for an expired token so I do not
      # want to limit the catching here.  All the errors for gdata.client
      # functionality extend gdata.client.Error (I do not see one that is
      # obviously about an expired token).
      return False

  def _GenerateTrackerToken(self):
    """Generate a new Tracker token from credentials."""
    # pylint: disable=W0404
    import gdata.client

    oper.Warning('Tracker token not valid.  Will try to generate a new one.')
    self.creds.LoadCreds(CRED_FILE)

    try:
      self.it_client.ClientLogin(self.creds.user, self.creds.password,
                                 source='Package Status', service='code',
                                 account_type='GOOGLE')
      self.creds.SetTrackerAuthToken(self.it_client.auth_token.token_string)

      oper.Notice('New Tracker token generated.')
      return True
    except gdata.client.BadAuthentication:
      oper.Error('Credentials from %r not accepted.'
                 '  Unable to generate new Tracker token.' % CRED_FILE)
      return False

  def Run(self):
    """Validate existing auth token or generate new one from credentials."""
    # pylint: disable=W0404
    import chromite.lib.gdata_lib as gdata_lib
    import gdata.spreadsheet.service

    self.creds = gdata_lib.Creds()
    self.gd_client = gdata.spreadsheet.service.SpreadsheetsService()
    self.it_client = gdata.projecthosting.client.ProjectHostingClient()

    self._LoadTokenFile()

    if not self._ValidateTrackerToken():
      if not self._GenerateTrackerToken():
        oper.Die('Failed to validate or generate Tracker token.')

    if not self._ValidateDocsToken():
      if not self._GenerateDocsToken():
        oper.Die('Failed to validate or generate Docs token.')

    self._SaveTokenFile()


def _CreateParser():
  usage = 'Usage: %prog'
  epilog = ('\n'
            'Run outside of chroot to validate the gdata '
            'token file at %r or update it if it has expired.\n'
            'To update the token file there must be a valid '
            'credentials file at %r.\n'
            'If run inside chroot the updated token file is '
            'still valid but will not be preserved if chroot\n'
            'is deleted.\n' %
            (TOKEN_FILE, CRED_FILE))

  return optparse.OptionParser(usage=usage, epilog=epilog)


def main(args):
  """Main function."""
  # Create a copy of args just to be safe.
  args = list(args)

  # No actual options used, but --help is still supported.
  parser = _CreateParser()
  (_options, args) = parser.parse_args(args)

  if args:
    parser.print_help()
    oper.Die('No arguments allowed.')

  if build_lib.IsInsideChroot():
    InsideChroot().Run()
  else:
    OutsideChroot(args).Run()
