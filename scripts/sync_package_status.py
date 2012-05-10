#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Synchronize issues in Package Status spreadsheet with Issue Tracker."""

import optparse
import os
import sys

from chromite.lib import cros_build_lib as cros_lib
from chromite.lib import gdata_lib
from chromite.lib import operation
from chromite.lib import upgrade_table as utable
from chromite.scripts import upload_package_status as ups

# pylint: disable=W0201,R0904

PROJECT_NAME = 'chromium-os'

PKGS_WS_NAME = 'Packages'

CROS_ORG = 'chromium.org'
CHROMIUMOS_SITE = 'http://www.%s/%s' % (CROS_ORG, PROJECT_NAME)
PKG_UPGRADE_PAGE = '%s/gentoo-package-upgrade-process' % CHROMIUMOS_SITE
DOCS_SITE = 'https://docs.google.com/a'

COL_PACKAGE = gdata_lib.PrepColNameForSS(utable.UpgradeTable.COL_PACKAGE)
COL_TEAM = gdata_lib.PrepColNameForSS('Team/Lead')
COL_OWNER = gdata_lib.PrepColNameForSS('Owner')
COL_TRACKER = gdata_lib.PrepColNameForSS('Tracker')

ARCHES = ('amd64', 'arm', 'x86')

oper = operation.Operation('sync_package_status')


def _GetPkgSpreadsheetURL(ss_key):
  return '%s/%s/spreadsheet/ccc?key=%s' % (DOCS_SITE, CROS_ORG, ss_key)


class SyncError(RuntimeError):
  """Extend RuntimeError for use in this module."""


class Syncer(object):
  """Class to manage synchronizing between spreadsheet and Tracker."""

  # Map spreadsheet team names to Tracker team values.
  VALID_TEAMS = {'build': 'BuildRelease',
                 'kernel': 'Kernel',
                 'security': 'Security',
                 'system': 'Systems',
                 'ui': 'UI',
                 }
  UPGRADE_STATES = set([utable.UpgradeTable.STATE_NEEDS_UPGRADE,
                        utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_PATCHED,
                        utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_DUPLICATED,
                        ])

  __slots__ = (
    'default_owner',  # Default owner to use when creating issues
    'owners',         # Set of owners to select (None means no filter)
    'pretend',        # If True, make no real changes
    'scomm',          # SpreadsheetComm
    'tcomm',          # TrackerComm
    'teams',          # Set of teams to select (None means no filter)
    'tracker_col_ix', # Index of Tracker column in spreadsheet
    'verbose',        # Verbose boolean
    )

  def __init__(self, tcomm, scomm, pretend=False, verbose=False):
    self.tcomm = tcomm
    self.scomm = scomm

    self.tracker_col_ix = None

    self.teams = None
    self.owners = None
    self.default_owner = None

    self.pretend = pretend
    self.verbose = verbose

  def _ReduceTeamName(self, team):
    """Translate |team| from spreadsheet/commandline name to short name.

    For example:  build/bdavirro --> build, build --> build
    """
    if team:
      return team.lower().split('/')[0]
    return None

  def SetTeamFilter(self, teamarg):
    """Set team filter using colon-separated team names in |teamarg|

    Resulting filter in self.teams is set of "reduced" team names.
    Examples:
      'build:system:ui' --> set(['build', 'system', 'ui'])
      'Build:system:UI' --> set(['build', 'system', 'ui'])

    If an invalid team name is given oper.Die is called with explanation.
    """
    if teamarg:
      teamlist = []
      for team in teamarg.split(':'):
        t = self._ReduceTeamName(team)
        if t in self.VALID_TEAMS:
          teamlist.append(t)
        else:
          oper.Die('Invalid team name "%s".  Choose from: %s' %
                   (team, ','.join(self.VALID_TEAMS.keys())))
      self.teams = set(teamlist)
    else:
      self.teams = None

  def _ReduceOwnerName(self, owner):
    """Translate |owner| from spreadsheet/commandline name to short name.

    For example:  joe@chromium.org -> joe, joe --> joe
    """
    if owner:
      return owner.lower().split('@')[0]
    return None

  def SetOwnerFilter(self, ownerarg):
    """Set owner filter using colon-separated owner names in |ownerarg|."""
    if ownerarg:
      self.owners = set([self._ReduceOwnerName(o) for o in ownerarg.split(':')])
    else:
      self.owners = None

  def SetDefaultOwner(self, default_owner):
    """Use |default_owner| as isue owner when none set in spreadsheet."""
    if default_owner and default_owner == 'me':
      self.default_owner = os.environ['USER']
    else:
      self.default_owner = default_owner

  def _RowPassesFilters(self, row):
    """Return true if |row| passes any team/owner filters."""
    if self.teams:
      team = self._ReduceTeamName(row[COL_TEAM])
      if team not in self.teams:
        return False

    if self.owners:
      owner = self._ReduceOwnerName(row[COL_OWNER])
      if owner not in self.owners:
        return False

    return True

  def Sync(self):
    """Do synchronization between Spreadsheet and Tracker."""

    self.tracker_col_ix = self.scomm.GetColumnIndex('Tracker')

    errors = []

    # Go over each row in Spreadsheet.  Row index starts at 2
    # because spreadsheet rows start at 1 and we don't want the header row.
    rows = self.scomm.GetRows()
    for rowIx, row in enumerate(rows, start=self.scomm.ROW_NUMBER_OFFSET):
      if not self._RowPassesFilters(row):
        oper.Info('\nSkipping row %d, pkg: %r (team=%s, owner=%s) ...' %
                  (rowIx, row[COL_PACKAGE], row[COL_TEAM], row[COL_OWNER]))
        continue

      oper.Info('\nProcessing row %d, pkg: %r (team=%s, owner=%s) ...' %
                (rowIx, row[COL_PACKAGE], row[COL_TEAM], row[COL_OWNER]))

      try:
        new_issue = self._GenIssueForRow(row)
        old_issue_id = self._GetRowTrackerId(row)

        if new_issue and not old_issue_id:
          self._CreateRowIssue(rowIx, row, new_issue)
        elif not new_issue and old_issue_id:
          self._ClearRowIssue(rowIx, row)
        else:
          # Nothing to do for this package.
          reason = 'already has issue' if old_issue_id else 'no upgrade needed'
          oper.Notice('Nothing to do for row %d, package %r: %s.' %
                      (rowIx, row[COL_PACKAGE], reason))
      except SyncError:
        errors.append('Error processing row %d, pkg: %r.  See above.' %
                      (rowIx, row[COL_PACKAGE]))

    if errors:
      raise SyncError('\n'.join(errors))

  def _GetRowValue(self, row, colName, arch=None):
    """Get value from |row| at |colName|, adjusted for |arch|"""
    if arch:
      colName = utable.UpgradeTable.GetColumnName(colName, arch=arch)
    colName = gdata_lib.PrepColNameForSS(colName)
    return row[colName]

  def _GenIssueForRow(self, row):
    """Generate an Issue object for |row| if applicable"""
    # Row needs an issue if it "needs upgrade" on any platform.
    statuses = {}
    needs_issue = False
    for arch in ARCHES:
      state = self._GetRowValue(row, utable.UpgradeTable.COL_STATE, arch)
      statuses[arch] = state
      if state in self.UPGRADE_STATES:
        needs_issue = True

    if not needs_issue:
      return None

    pkg = row[COL_PACKAGE]
    team = self._ReduceTeamName(row[COL_TEAM])
    if not team:
      oper.Error('Unable to create Issue for package "%s" because no '
                 'team value is specified.' % pkg)
      raise SyncError()

    team_label = self.VALID_TEAMS[team]
    labels = ['Type-Task',
              'Area-LinuxUserSpace',
              'Pri-2',
              'Team-%s' % team_label,
              ]

    owner = self._ReduceOwnerName(row[COL_OWNER])
    status = 'Untriaged'
    if owner:
      owner = owner + '@chromium.org'
      status = 'Available'
    elif self.default_owner:
      owner = self.default_owner + '@chromium.org.'
    else:
      owner = None # Rather than ''

    title = '%s package needs upgrade from upstream Portage' % pkg

    lines = ['The %s package can be upgraded from upstream Portage' % pkg,
             '',
             'At this moment the status on each arch is as follows:',
             ]

    for arch in sorted(statuses):
      arch_status = statuses[arch]
      if arch_status:
        # Get all versions for this arch right now.
        curr_ver_col = utable.UpgradeTable.COL_CURRENT_VER
        curr_ver = self._GetRowValue(row, curr_ver_col, arch)
        stable_upst_ver_col = utable.UpgradeTable.COL_STABLE_UPSTREAM_VER
        stable_upst_ver = self._GetRowValue(row, stable_upst_ver_col, arch)
        latest_upst_ver_col = utable.UpgradeTable.COL_LATEST_UPSTREAM_VER
        latest_upst_ver = self._GetRowValue(row, latest_upst_ver_col, arch)

        arch_vers = ['Current version: %s' % curr_ver,
                     'Stable upstream version: %s' % stable_upst_ver,
                     'Latest upstream version: %s' % latest_upst_ver]
        lines.append('  On %s: %s' % (arch, arch_status))
        lines.append('    %s' % ', '.join(arch_vers))
      else:
        lines.append('  On %s: not used' % arch)

    lines.append('')
    lines.append('Check the latest status for this package, including '
                 'which upstream versions are available, at:\n  %s' %
                 _GetPkgSpreadsheetURL(self.scomm.ss_key))
    lines.append('For help upgrading see: %s' % PKG_UPGRADE_PAGE)

    summary = '\n'.join(lines)

    issue = gdata_lib.Issue(title=title,
                            summary=summary,
                            status=status,
                            owner=owner,
                            labels=labels,
                            )
    return issue

  def _GetRowTrackerId(self, row):
    """Get the tracker issue id in |row| if it exists, return None otherwise."""
    tracker_val = row[COL_TRACKER]
    if tracker_val:
      return int(tracker_val)

    return None

  def _CreateRowIssue(self, rowIx, row, issue):
    """Create a Tracker issue for |issue|, insert into |row| at |rowIx|"""

    pkg = row[COL_PACKAGE]
    if not self.pretend:
      oper.Info('Creating Tracker issue for package %s with details:\n%s' %
                (pkg, issue))

      # Before actually creating the Tracker issue, confirm that writing
      # to this spreadsheet row is going to work.
      try:
        self.scomm.ClearCellValue(rowIx, self.tracker_col_ix)
      except gdata_lib.SpreadsheetError as ex:
        oper.Error('Unable to write to row %d, package %r.  Aborting issue'
                   ' creation.  Error was:\n%s' % (rowIx, pkg, ex))
        raise SyncError

      issue_id = self.tcomm.CreateTrackerIssue(issue)
      oper.Info('Inserting new Tracker issue %d for package %s' %
                (issue_id, pkg))
      ss_issue_val = self._GenSSLinkToIssue(issue_id)

      # This really should not fail since write access was checked before.
      try:
        self.scomm.ReplaceCellValue(rowIx, self.tracker_col_ix, ss_issue_val)
        oper.Notice('Created Tracker issue %d for row %d, package %r' %
                    (issue_id, rowIx, pkg))
      except gdata_lib.SpreadsheetError as ex:
        oper.Error('Failed to write link to new issue %d into'
                   ' row %d, package %r:\n%s' %
                   (issue_id, rowIx, pkg, ex))
        oper.Error('This means that the spreadsheet will have no record of'
                   ' this Tracker Issue and will create one again next time'
                   ' unless the spreadsheet is edited by hand!')
        raise SyncError

    else:
      oper.Notice('Would create and insert issue for row %d, package %r' %
                  (rowIx, pkg))
      oper.Info('Issue would be as follows:\n%s' % issue)

  def _GenSSLinkToIssue(self, issue_id):
    """Create the spreadsheet hyperlink format for |issue_id|"""
    return '=hyperlink("crosbug.com/%d";"%d")' % (issue_id, issue_id)

  def _ClearRowIssue(self, rowIx, row):
    """Clear the Tracker cell for row at |rowIx|"""

    pkg = row[COL_PACKAGE]
    if not self.pretend:
      try:
        self.scomm.ClearCellValue(rowIx, self.tracker_col_ix)
        oper.Notice('Cleared Tracker issue from row %d, package %r' %
                    (rowIx, pkg))
      except gdata_lib.SpreadsheetError as ex:
        oper.Error('Error while clearing Tracker issue for'
                   ' row %d, package %r:\n%s' % (rowIx, pkg, ex))
        raise SyncError

    else:
      oper.Notice('Would clear Tracker issue from row %d, package %r' %
                  (rowIx, pkg))


def PrepareCreds(cred_file, token_file, email):
  """Return a Creds object from given credentials.

  If |email| is given, the Creds object will contain that |email|
  and a password entered at a prompt.

  Otherwise, if |token_file| is given then the Creds object will have
  the auth_token from that file.

  Otherwise, if |cred_file| is given then the Creds object will have
  the email/password from that file.
  """

  creds = gdata_lib.Creds()

  if email:
    creds.SetCreds(email)
  elif token_file and os.path.exists(token_file):
    creds.LoadAuthToken(token_file)
  elif cred_file and os.path.exists(cred_file):
    creds.LoadCreds(cred_file)

  return creds


def _CreateOptParser():
  """Create the optparser.parser object for command-line args."""
  usage = 'Usage: %prog [options]'
  epilog = ("""
Use this script to synchronize tracker issues between the package status
spreadsheet and the chromium-os Tracker.  It uses the "Tracker" column of the
package spreadsheet.  If a package needs an upgrade and has no tracker issue in
that column then a tracker issue is created.  If it does not need an upgrade
then that column is cleared.

Credentials must be specified using --auth-token-file, --cred-file or --email.
The first two have default values which you can rely on if valid, the latter
will prompt for your password.  If you specify --email you will be given a
chance to save your email/password out as a credentials file for next time.

Uses spreadsheet key %(ss_key)s, worksheet "%(ws_name)s".
(if --test-spreadsheet is set then spreadsheet
%(test_ss_key)s is used).

Use the --team and --owner options to operate only on packages assigned to
particular owners or teams.  Generally, running without a team or owner filter
is not intended, so use --team=all and/or --owner=all.

Issues will be assigned to the owner of the package in the spreadsheet, if
available.  If not, the owner defaults to value given to --default-owner.

The --owner and --default-owner options accept "me" as an argument, which is
only useful if your username matches your chromium account name.

""" %
            {'ss_key': ups.REAL_SS_KEY, 'ws_name': PKGS_WS_NAME,
             'test_ss_key': ups.TEST_SS_KEY}
            )

  class MyOptParser(optparse.OptionParser):
    """Override default epilog formatter, which strips newlines."""
    def format_epilog(self, formatter):
      return self.epilog

  teamhelp = '[%s]' % ', '.join(Syncer.VALID_TEAMS.keys())

  parser = MyOptParser(usage=usage, epilog=epilog)
  parser.add_option('--auth-token-file', dest='token_file', type='string',
                    action='store', default=gdata_lib.TOKEN_FILE,
                    help='File for reading/writing Docs auth token.'
                    ' [default: "%default"]')
  parser.add_option('--cred-file', dest='cred_file', type='string',
                    action='store', default=gdata_lib.CRED_FILE,
                    help='Path to gdata credentials file [default: "%default"]')
  parser.add_option('--email', dest='email', type='string',
                    action='store', default=None,
                    help="Email for Google Doc/Tracker user")
  parser.add_option('--pretend', dest='pretend', action='store_true',
                    default=False,
                    help='Do not make any actual changes.')
  parser.add_option('--team', dest='team', type='string', action='store',
                    default=None,
                    help='Filter by team; colon-separated %s' % teamhelp)
  parser.add_option('--default-owner', dest='default_owner',
                    type='string', action='store', default=None,
                    help='Specify issue owner to use when package has no owner')
  parser.add_option('--owner', dest='owner', type='string', action='store',
                    default=None,
                    help='Filter by package owner;'
                    ' colon-separated chromium.org accounts')
  parser.add_option('--test-spreadsheet', dest='test_ss',
                    action='store_true', default=False,
                    help='Sync to the testing spreadsheet (implies --pretend).')
  parser.add_option('--verbose', dest='verbose', action='store_true',
                    default=False,
                    help='Enable verbose output (for debugging)')

  return parser


def _CheckOptions(options):
  """Vet the options."""
  me = os.environ['USER']

  if not options.email and not os.path.exists(options.cred_file):
    options.email = me
    oper.Notice('Assuming your chromium email is %s@chromium.org.'
                '  Override with --email.' % options.email)

  if not options.team and not options.owner:
    oper.Notice('Without --owner or --team filters this will run for all'
                ' packages in the spreadsheet (same as --team=all).')
    prompt = 'Are you sure you want to run for all packages?'
    if 'no' == cros_lib.YesNoPrompt(default='no', prompt=prompt):
      sys.exit(0)

  if options.team and options.team == 'all':
    options.team = None

  if options.owner and options.owner == 'all':
    options.owner = None

  if options.owner and options.owner == 'me':
    options.owner = me
    oper.Notice('Using %r for owner filter (from $USER envvar)' % options.owner)

  if options.test_ss and not options.pretend:
    oper.Notice('Running in --pretend mode because of --test-spreadsheet')
    options.pretend = True


def main(argv):
  """Main function."""
  parser = _CreateOptParser()
  (options, _args) = parser.parse_args(argv)

  oper.verbose = options.verbose

  _CheckOptions(options)

  ss_key = ups.TEST_SS_KEY if options.test_ss else ups.REAL_SS_KEY

  # Prepare credentials for Docs and Tracker access.
  creds = PrepareCreds(options.cred_file, options.token_file, options.email)

  scomm = gdata_lib.SpreadsheetComm()
  scomm.Connect(creds, ss_key, PKGS_WS_NAME, source='Sync Package Status')
  tcomm = gdata_lib.TrackerComm()
  tcomm.Connect(creds, PROJECT_NAME, source='Sync Package Status')

  oper.Notice('Syncing between Tracker and spreadsheet %s' % ss_key)
  syncer = Syncer(tcomm, scomm,
                  pretend=options.pretend, verbose=options.verbose)

  if options.team:
    syncer.SetTeamFilter(options.team)
  if options.owner:
    syncer.SetOwnerFilter(options.owner)
  if options.default_owner:
    syncer.SetDefaultOwner(options.default_owner)

  try:
    syncer.Sync()
  except SyncError as ex:
    oper.Die(str(ex))

  # If --email, which is only effective when run interactively (because the
  # password must be entered), give the option of saving to a creds file for
  # next time.
  if options.email and options.cred_file:
    prompt = ('Do you want to save credentials for next time to %r?' %
              options.cred_file)
    if 'yes' == cros_lib.YesNoPrompt(default='no', prompt=prompt):
      creds.StoreCreds(options.cred_file)
      oper.Notice('Be sure to save the creds file to the same location'
                  ' outside your chroot so it will also be used with'
                  ' future chroots.')
