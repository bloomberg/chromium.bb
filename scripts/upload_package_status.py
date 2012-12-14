#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Support uploading a csv file to a Google Docs spreadsheet."""

import optparse
import os

from chromite.lib import gdata_lib
from chromite.lib import table
from chromite.lib import operation
from chromite.lib import upgrade_table as utable
from chromite.scripts import merge_package_status as mps

REAL_SS_KEY = '0AsXDKtaHikmcdEp1dVN1SG1yRU1xZEw1Yjhka2dCSUE'
TEST_SS_KEY = '0AsXDKtaHikmcdDlQMjI3ZDdPVGc4Rkl3Yk5OLWxjR1E'
PKGS_WS_NAME = 'Packages'
DEPS_WS_NAME = 'Dependencies'

oper = operation.Operation('upload_package_status')


class Uploader(object):
  """Uploads portage package status data from csv file to Google spreadsheet."""

  __slots__ = ('_creds',          # gdata_lib.Creds object
               '_scomm',          # gdata_lib.SpreadsheetComm object
               '_ss_row_cache',   # dict with key=pkg, val=SpreadsheetRow obj
               '_csv_table',      # table.Table of csv rows
               )

  ID_COL = utable.UpgradeTable.COL_PACKAGE
  SS_ID_COL = gdata_lib.PrepColNameForSS(ID_COL)
  SOURCE = 'Uploaded from CSV'

  def __init__(self, creds, table_obj):
    self._creds = creds
    self._csv_table = table_obj
    self._scomm = None
    self._ss_row_cache = None

  def _GetSSRowForPackage(self, package):
    """Return the SpreadsheetRow corresponding to Package=|package|."""
    if package in self._ss_row_cache:
      row = self._ss_row_cache[package]

      if isinstance(row, list):
        raise LookupError('More than one row in spreadsheet with Package=%s' %
                          package)

      return row

    return None

  def Upload(self, ss_key, ws_name):
    """Upload |_csv_table| to the given Google Spreadsheet.

    The spreadsheet is identified the spreadsheet key |ss_key|.
    The worksheet within that spreadsheet is identified by the
    worksheet name |ws_name|.
    """
    if self._scomm:
      self._scomm.SetCurrentWorksheet(ws_name)
    else:
      self._scomm = gdata_lib.SpreadsheetComm()
      self._scomm.Connect(self._creds, ss_key, ws_name,
                          source='Upload Package Status')

    oper.Notice('Caching rows for worksheet %r.' % self._scomm.ws_name)
    self._ss_row_cache = self._scomm.GetRowCacheByCol(self.SS_ID_COL)

    oper.Notice('Uploading changes to worksheet "%s" of spreadsheet "%s" now.' %
                (self._scomm.ws_name, self._scomm.ss_key))

    oper.Info('Details by package: S=Same, C=Changed, A=Added, D=Deleted')
    rows_unchanged, rows_updated, rows_inserted = self._UploadChangedRows()
    rows_deleted, rows_with_owner_deleted = self._DeleteOldRows()

    oper.Notice('Final row stats for worksheet "%s"'
                ': %d changed, %d added, %d deleted, %d same.' %
                (self._scomm.ws_name, rows_updated, rows_inserted,
                 rows_deleted, rows_unchanged))
    if rows_with_owner_deleted:
      oper.Warning('%d rows with owner entry deleted, see above warnings.' %
                   rows_with_owner_deleted)
    else:
      oper.Notice('No rows with owner entry were deleted.')

  def _UploadChangedRows(self):
    """Upload all rows in table that need to be changed in spreadsheet."""
    rows_unchanged, rows_updated, rows_inserted = (0, 0, 0)

    # Go over all rows in csv table.  Identify existing row by the 'Package'
    # column.  Either update existing row or create new one.
    for csv_row in self._csv_table:
      # Seed new row values from csv_row values, with column translation.
      new_row = dict((gdata_lib.PrepColNameForSS(key),
                      csv_row[key]) for key in csv_row)

      # Retrieve row values already in spreadsheet, along with row index.
      csv_package = csv_row[self.ID_COL]
      ss_row = self._GetSSRowForPackage(csv_package)

      if ss_row:
        changed = [] # Gather changes for log message.

        # Check each key/value in new_row to see if it is different from what
        # is already in spreadsheet (ss_row).  Keep only differences to get
        # the row delta.
        row_delta = {}
        for col in new_row:
          if col in ss_row:
            ss_val = ss_row[col]
            new_val = new_row[col]
            if (ss_val or new_val) and ss_val != new_val:
              changed.append('%s="%s"->"%s"' % (col, ss_val, new_val))
              row_delta[col] = new_val

        if row_delta:
          self._scomm.UpdateRowCellByCell(ss_row.ss_row_num,
                                          gdata_lib.PrepRowForSS(row_delta))
          rows_updated += 1
          oper.Info('C %-30s: %s' % (csv_package, ', '.join(changed)))
        else:
          rows_unchanged += 1
          oper.Info('S %-30s:' % csv_package)
      else:
        self._scomm.InsertRow(gdata_lib.PrepRowForSS(new_row))
        rows_inserted += 1
        row_descr_list = []
        for col in sorted(new_row.keys()):
          if col != self.ID_COL:
            row_descr_list.append('%s="%s"' % (col, new_row[col]))
        oper.Info('A %-30s: %s' % (csv_package, ', '.join(row_descr_list)))

    return (rows_unchanged, rows_updated, rows_inserted)

  def _DeleteOldRows(self):
    """Delete all rows from spreadsheet that not found in table."""
    oper.Notice('Checking for rows in worksheet that should be deleted now.')

    rows_deleted, rows_with_owner_deleted = (0, 0)

    # Also need to delete rows in spreadsheet that are not in csv table.
    ss_rows = self._scomm.GetRows()
    for ss_row in ss_rows:
      ss_package = gdata_lib.ScrubValFromSS(ss_row[self.SS_ID_COL])

      # See whether this row is in csv table.
      csv_rows = self._csv_table.GetRowsByValue({ self.ID_COL: ss_package })
      if not csv_rows:
        # Row needs to be deleted from spreadsheet.
        owner_val = None
        owner_notes_val = None
        row_descr_list = []
        for col in sorted(ss_row.keys()):
          if col == 'owner':
            owner_val = ss_row[col]
          if col == 'ownernotes':
            owner_notes_val = ss_row[col]

          # Don't include ID_COL value in description, it is in prefix already.
          if col != self.SS_ID_COL:
            val = ss_row[col]
            row_descr_list.append('%s="%s"' % (col, val))

        oper.Info('D %-30s: %s' % (ss_package, ', '.join(row_descr_list)))
        if owner_val or owner_notes_val:
          rows_with_owner_deleted += 1
          oper.Notice('WARNING: Deleting spreadsheet row with owner entry:\n' +
                      '  %-30s: Owner=%s, Owner Notes=%s' %
                      (ss_package, owner_val, owner_notes_val))

        self._scomm.DeleteRow(ss_row.ss_row_obj)
        rows_deleted += 1

    return (rows_deleted, rows_with_owner_deleted)


def LoadTable(table_file):
  """Load csv |table_file| into a table.  Return table."""
  oper.Notice('Loading csv table from "%s".' % (table_file))
  csv_table = table.Table.LoadFromCSV(table_file)
  return csv_table


def PrepareCreds(cred_file, token_file, email, password):
  """Return a Creds object from given credentials.

  If |email| is given, the Creds object will contain that |email|
  and either the given |password| or one entered at a prompt.

  Otherwise, if |token_file| is given then the Creds object will have
  the auth_token from that file.

  Otherwise, if |cred_file| is given then the Creds object will have
  the email/password from that file.
  """

  creds = gdata_lib.Creds()

  if email:
    creds.SetCreds(email, password)
  elif token_file and os.path.exists(token_file):
    creds.LoadAuthToken(token_file)
  elif cred_file and os.path.exists(cred_file):
    creds.LoadCreds(cred_file)

  return creds

def main(argv):
  """Main function."""
  usage = 'Usage: %prog [options] csv_file'
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('--auth-token-file', dest='token_file', type='string',
                    action='store', default=None,
                    help='File for reading/writing Docs auth token.')
  parser.add_option('--cred-file', dest='cred_file', type='string',
                    action='store', default=None,
                    help='File for reading/writing Docs login email/password.')
  parser.add_option('--email', dest='email', type='string',
                    action='store', default=None,
                    help='Email for Google Doc user')
  parser.add_option('--password', dest='password', type='string',
                    action='store', default=None,
                    help='Password for Google Doc user')
  parser.add_option('--ss-key', dest='ss_key', type='string',
                    action='store', default=None,
                    help='Key of spreadsheet to upload to')
  parser.add_option('--test-spreadsheet', dest='test_ss',
                    action='store_true', default=False,
                    help='Upload to the testing spreadsheet.')
  parser.add_option('--verbose', dest='verbose',
                    action='store_true', default=False,
                    help='Show details about packages.')

  (options, args) = parser.parse_args(argv)

  oper.verbose = options.verbose

  if len(args) < 1:
    parser.print_help()
    oper.Die('One csv_file is required.')

  # If email or password provided, the other is required.  If neither is
  # provided, then either token_file or cred_file must be provided and
  # be a real file.
  if options.email or options.password:
    if not (options.email and options.password):
      parser.print_help()
      oper.Die('The email/password options must be used together.')
  elif not ((options.cred_file and os.path.exists(options.cred_file)) or
            (options.token_file and os.path.exists(options.token_file))):
    parser.print_help()
    oper.Die('Without email/password, cred-file or auth-token-file'
             'must exist.')

  # --ss-key and --test-spreadsheet are mutually exclusive.
  if options.ss_key and options.test_ss:
    parser.print_help()
    oper.Die('Cannot specify --ss-key and --test-spreadsheet together.')

  # Prepare credentials for spreadsheet access.
  creds = PrepareCreds(options.cred_file, options.token_file,
                       options.email, options.password)

  # Load the given csv file.
  csv_table = LoadTable(args[0])

  # Prepare table for upload.
  mps.FinalizeTable(csv_table)

  # Prepare the Google Doc client for uploading.
  uploader = Uploader(creds, csv_table)

  ss_key = options.ss_key
  ws_names = [PKGS_WS_NAME, DEPS_WS_NAME]
  if not ss_key:
    if options.test_ss:
      ss_key = TEST_SS_KEY # For testing with backup spreadsheet
    else:
      ss_key = REAL_SS_KEY

  for ws_name in ws_names:
    uploader.Upload(ss_key, ws_name=ws_name)

  # If cred_file given and new credentials were used then write
  # credentials out to that location.
  if options.cred_file:
    creds.StoreCredsIfNeeded(options.cred_file)

  # If token_file path given and new auth token was used then
  # write auth_token out to that location.
  if options.token_file:
    creds.StoreAuthTokenIfNeeded(options.token_file)
