# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for gathering stats from builder runs."""

from __future__ import division
from __future__ import print_function

import datetime
import os
import re
import sys

from chromite.cbuildbot import cbuildbot_config
from chromite.cbuildbot import metadata_lib
from chromite.cbuildbot import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import gdata_lib
from chromite.lib import graphite
from chromite.lib import gs
from chromite.lib import table


# Useful config targets.
CQ_MASTER = constants.CQ_MASTER
PFQ_MASTER = constants.PFQ_MASTER
CANARY_MASTER = constants.CANARY_MASTER

# Bot types
CQ = constants.CQ
PRE_CQ = constants.PRE_CQ
PFQ = constants.PFQ_TYPE
CANARY = constants.CANARY_TYPE

# Number of parallel processes used when uploading/downloading GS files.
MAX_PARALLEL = 40

# The graphite graphs use seconds since epoch start as time value.
EPOCH_START = metadata_lib.EPOCH_START

# Formats we like for output.
NICE_DATE_FORMAT = metadata_lib.NICE_DATE_FORMAT
NICE_TIME_FORMAT = metadata_lib.NICE_TIME_FORMAT
NICE_DATETIME_FORMAT = metadata_lib.NICE_DATETIME_FORMAT

# Spreadsheet keys
# CQ master and slaves both use the same spreadsheet
CQ_SS_KEY = '0AsXDKtaHikmcdElQWVFuT21aMlFXVTN5bVhfQ2ptVFE'
PFQ_SS_KEY = '0AhFPeDq6pmwxdDdrYXk3cnJJV05jN3Zja0s5VjFfNlE'
CANARY_SS_KEY = '0AhFPeDq6pmwxdDBWM0t3YnYyeVFoM3VaRXNianc1VVE'


class GatherStatsError(Exception):
  """Base exception class for exceptions in this module."""


class DataError(GatherStatsError):
  """Any exception raised when an error occured while collectring data."""


class SpreadsheetError(GatherStatsError):
  """Raised when there is a problem with the stats spreadsheet."""


def _SendToCarbon(builds, token_funcs):
  """Send data for |builds| to Carbon/Graphite according to |token_funcs|.

  Args:
    builds: List of BuildData objects.
    token_funcs: List of functors that each take a BuildData object as the only
      argument and return a string.  Each line of data to send to Carbon is
      constructed by taking the strings returned from these functors and
      concatenating them using single spaces.
  """
  lines = [' '.join([str(func(b)) for func in token_funcs]) for b in builds]
  cros_build_lib.Info('Sending %d lines to Graphite now.', len(lines))
  graphite.SendToCarbon(lines)


# TODO(dgarrett): Discover list from Json. Will better track slave changes.
def _GetSlavesOfMaster(master_target):
  """Returns list of slave config names for given master config name.

  Args:
    master_target: Name of master config target.

  Returns:
    List of names of slave config targets.
  """
  master_config = cbuildbot_config.config[master_target]
  slave_configs = cbuildbot_config.GetSlavesForMaster(master_config)
  return sorted(slave_config.name for slave_config in slave_configs)


class StatsTable(table.Table):
  """Stats table for any specific target on a waterfall."""

  LINK_ROOT = ('https://uberchromegw.corp.google.com/i/%(waterfall)s/builders/'
               '%(target)s/builds')

  @staticmethod
  def _SSHyperlink(link, text):
    return '=HYPERLINK("%s", "%s")' % (link, text)

  def __init__(self, target, waterfall, columns):
    super(StatsTable, self).__init__(columns, target)
    self.target = target
    self.waterfall = waterfall

  def GetBuildLink(self, build_number):
    target = self.target.replace(' ', '%20')
    link = self.LINK_ROOT % {'waterfall': self.waterfall, 'target': target}
    link += '/%s' % build_number
    return link

  def GetBuildSSLink(self, build_number):
    link = self.GetBuildLink(build_number)
    return self._SSHyperlink(link, 'build %s' % build_number)


class SpreadsheetMasterTable(StatsTable):
  """Stats table for master builder that puts results in a spreadsheet."""
  # Bump this number whenever this class adds new data columns, or changes
  # the values of existing data columns.
  SHEETS_VERSION = 2

  # These must match up with the column names on the spreadsheet.
  COL_BUILD_NUMBER = 'build number'
  COL_BUILD_LINK = 'build link'
  COL_STATUS = 'status'
  COL_START_DATETIME = 'start datetime'
  COL_RUNTIME_MINUTES = 'runtime minutes'
  COL_WEEKDAY = 'weekday'
  COL_CHROMEOS_VERSION = 'chromeos version'
  COL_CHROME_VERSION = 'chrome version'
  COL_FAILED_STAGES = 'failed stages'
  COL_FAILURE_MESSAGE = 'failure message'

  # It is required that the ID_COL be an integer value.
  ID_COL = COL_BUILD_NUMBER

  COLUMNS = (
      COL_BUILD_NUMBER,
      COL_BUILD_LINK,
      COL_STATUS,
      COL_START_DATETIME,
      COL_RUNTIME_MINUTES,
      COL_WEEKDAY,
      COL_CHROMEOS_VERSION,
      COL_CHROME_VERSION,
      COL_FAILED_STAGES,
      COL_FAILURE_MESSAGE,
  )

  def __init__(self, target, waterfall, columns=None):
    columns = columns or []
    columns = list(SpreadsheetMasterTable.COLUMNS) + columns
    super(SpreadsheetMasterTable, self).__init__(target,
                                                 waterfall,
                                                 columns)

    self._slaves = []

  def _CreateAbortedRowDict(self, build_number):
    """Create a row dict to represent an aborted run of |build_number|."""
    return {
        self.COL_BUILD_NUMBER: str(build_number),
        self.COL_BUILD_LINK: self.GetBuildSSLink(build_number),
        self.COL_STATUS: 'aborted',
    }

  def AppendGapRow(self, build_number):
    """Append a row to represent a missing run of |build_number|."""
    self.AppendRow(self._CreateAbortedRowDict(build_number))

  def AppendBuildRow(self, build_data):
    """Append a row from the given |build_data|.

    Args:
      build_data: A BuildData object.
    """
    # First see if any build number gaps are in the table, and if so fill
    # them in.  This happens when a CQ run is aborted and never writes metadata.
    num_rows = self.GetNumRows()
    if num_rows:
      last_row = self.GetRowByIndex(num_rows - 1)
      last_build_number = int(last_row[self.COL_BUILD_NUMBER])

      for bn in range(build_data.build_number + 1, last_build_number):
        self.AppendGapRow(bn)

    row = self._GetBuildRow(build_data)

    #Use a separate column for each slave.
    slaves = build_data.slaves
    for slave_name in slaves:
      # This adds the slave to our local data, but doesn't add a missing
      # column to the spreadsheet itself.
      self._EnsureSlaveKnown(slave_name)

    # Now add the finished row to this table.
    self.AppendRow(row)

  def _GetBuildRow(self, build_data):
    """Fetch a row dictionary from |build_data|

    Returns:
      A dictionary of the form {column_name: value}
    """
    build_number = build_data.build_number
    build_link = self.GetBuildSSLink(build_number)

    # For datetime.weekday(), 0==Monday and 6==Sunday.
    is_weekday = build_data.start_datetime.weekday() in range(5)

    row = {
        self.COL_BUILD_NUMBER: str(build_number),
        self.COL_BUILD_LINK: build_link,
        self.COL_STATUS: build_data.status,
        self.COL_START_DATETIME: build_data.start_datetime_str,
        self.COL_RUNTIME_MINUTES: str(build_data.runtime_minutes),
        self.COL_WEEKDAY: str(is_weekday),
        self.COL_CHROMEOS_VERSION: build_data.chromeos_version,
        self.COL_CHROME_VERSION: build_data.chrome_version,
        self.COL_FAILED_STAGES: ' '.join(build_data.GetFailedStages()),
        self.COL_FAILURE_MESSAGE: build_data.failure_message,
    }

    slaves = build_data.slaves
    for slave_name in slaves:
      slave = slaves[slave_name]
      slave_url = slave.get('dashboard_url')

      # For some reason status in slaves uses pass/fail instead of
      # passed/failed used elsewhere in metadata.
      translate_dict = {'fail': 'failed', 'pass': 'passed'}
      slave_status = translate_dict.get(slave['status'], slave['status'])

      # Bizarrely, dashboard_url is not always set for slaves that pass.
      # Only sometimes.  crbug.com/350939.
      if slave_url:
        row[slave_name] = self._SSHyperlink(slave_url, slave_status)
      else:
        row[slave_name] = slave_status

    return row

  def _EnsureSlaveKnown(self, slave_name):
    """Ensure that a slave builder name is known.

    Args:
      slave_name: The name of the slave builder (aka spreadsheet column name).
    """
    if not self.HasColumn(slave_name):
      self._slaves.append(slave_name)
      self.AppendColumn(slave_name)

  def GetSlaves(self):
    """Get the list of slave builders which has been discovered so far.

    This list is only fully populated when all row data has been fully
    populated.

    Returns:
      List of column names for slave builders.
    """
    return self._slaves[:]


class PFQMasterTable(SpreadsheetMasterTable):
  """Stats table for the PFQ Master."""
  SS_KEY = PFQ_SS_KEY

  WATERFALL = 'chromeos'
  # Must match up with name in waterfall.
  TARGET = 'x86-generic nightly chromium PFQ'

  WORKSHEET_NAME = 'PFQMasterData'

  # Bump this number whenever this class adds new data columns, or changes
  # the values of existing data columns.
  SHEETS_VERSION = SpreadsheetMasterTable.SHEETS_VERSION + 1

  # These columns are in addition to those inherited from
  # SpreadsheetMasterTable
  COLUMNS = ()

  def __init__(self):
    super(PFQMasterTable, self).__init__(PFQMasterTable.TARGET,
                                         PFQMasterTable.WATERFALL,
                                         list(PFQMasterTable.COLUMNS))


class CanaryMasterTable(SpreadsheetMasterTable):
  """Stats table for the Canary Master."""
  SS_KEY = PFQ_SS_KEY

  WATERFALL = 'chromeos'
  # Must match up with name in waterfall.
  TARGET = 'Canary master'

  WORKSHEET_NAME = 'CanaryMasterData'

  # Bump this number whenever this class adds new data columns, or changes
  # the values of existing data columns.
  SHEETS_VERSION = SpreadsheetMasterTable.SHEETS_VERSION

  # These columns are in addition to those inherited from
  # SpreadsheetMasterTable
  COLUMNS = ()

  def __init__(self):
    super(CanaryMasterTable, self).__init__(CanaryMasterTable.TARGET,
                                            CanaryMasterTable.WATERFALL,
                                            list(CanaryMasterTable.COLUMNS))


class CQMasterTable(SpreadsheetMasterTable):
  """Stats table for the CQ Master."""
  WATERFALL = 'chromeos'
  TARGET = 'CQ master' # Must match up with name in waterfall.

  WORKSHEET_NAME = 'CQMasterData'

  # Bump this number whenever this class adds new data columns, or changes
  # the values of existing data columns.
  SHEETS_VERSION = SpreadsheetMasterTable.SHEETS_VERSION + 2

  COL_CL_COUNT = 'cl count'
  COL_CL_SUBMITTED_COUNT = 'cls submitted'
  COL_CL_REJECTED_COUNT = 'cls rejected'

  # These columns are in addition to those inherited from
  # SpreadsheetMasterTable
  COLUMNS = (
      COL_CL_COUNT,
      COL_CL_SUBMITTED_COUNT,
      COL_CL_REJECTED_COUNT,
  )

  def __init__(self):
    super(CQMasterTable, self).__init__(CQMasterTable.TARGET,
                                        CQMasterTable.WATERFALL,
                                        list(CQMasterTable.COLUMNS))

  def _GetBuildRow(self, build_data):
    """Fetch a row dictionary from |build_data|

    Returns:
      A dictionary of the form {column_name: value}
    """
    row = super(CQMasterTable, self)._GetBuildRow(build_data)
    row[self.COL_CL_COUNT] = str(build_data.count_changes)

    cl_actions = [metadata_lib.CLActionTuple(*x)
                  for x in build_data['cl_actions']]
    submitted_cl_count = len([x for x in cl_actions if
                              x.action == constants.CL_ACTION_SUBMITTED])
    rejected_cl_count = len([x for x in cl_actions
                             if x.action == constants.CL_ACTION_KICKED_OUT])
    row[self.COL_CL_SUBMITTED_COUNT] = str(submitted_cl_count)
    row[self.COL_CL_REJECTED_COUNT] = str(rejected_cl_count)
    return row


class SSUploader(object):
  """Uploads data from table object to Google spreadsheet."""

  __slots__ = (
      '_creds',          # gdata_lib.Creds object
      '_scomm',          # gdata_lib.SpreadsheetComm object
      'ss_key',          # Spreadsheet key string
  )

  SOURCE = 'Gathered from builder metadata'
  HYPERLINK_RE = re.compile(r'=HYPERLINK\("[^"]+", "([^"]+)"\)')
  DATETIME_FORMATS = ('%m/%d/%Y %H:%M:%S', NICE_DATETIME_FORMAT)

  def __init__(self, creds, ss_key):
    self._creds = creds
    self.ss_key = ss_key
    self._scomm = None

  @classmethod
  def _ValsEqual(cls, val1, val2):
    """Compare two spreadsheet values and return True if they are the same.

    This is non-trivial because of the automatic changes that Google Sheets
    does to values.

    Args:
      val1: New or old spreadsheet value to compare.
      val2: New or old spreadsheet value to compare.

    Returns:
      True if val1 and val2 are effectively the same, False otherwise.
    """
    # An empty string sent to spreadsheet comes back as None.  In any case,
    # treat two false equivalents as equal.
    if not (val1 or val2):
      return True

    # If only one of the values is set to anything then they are not the same.
    if bool(val1) != bool(val2):
      return False

    # If values are equal then we are done.
    if val1 == val2:
      return True

    # Ignore case differences.  This is because, for example, the
    # spreadsheet automatically changes "True" to "TRUE".
    if val1 and val2 and val1.lower() == val2.lower():
      return True

    # If either value is a HYPERLINK, then extract just the text for comparison
    # because that is all the spreadsheet says the value is.
    match = cls.HYPERLINK_RE.search(val1)
    if match:
      return match.group(1) == val2
    match = cls.HYPERLINK_RE.search(val2)
    if match:
      return match.group(2) == val1

    # See if the strings are two different representations of the same datetime.
    dt1, dt2 = None, None
    for dt_format in cls.DATETIME_FORMATS:
      try:
        dt1 = datetime.datetime.strptime(val1, dt_format)
      except ValueError:
        pass
      try:
        dt2 = datetime.datetime.strptime(val2, dt_format)
      except ValueError:
        pass
    if dt1 and dt2 and dt1 == dt2:
      return True

    # If we get this far then the values are just different.
    return False

  def _Connect(self, ws_name):
    """Establish connection to specific worksheet.

    Args:
      ws_name: Worksheet name.
    """
    if self._scomm:
      self._scomm.SetCurrentWorksheet(ws_name)
    else:
      self._scomm = gdata_lib.SpreadsheetComm()
      self._scomm.Connect(self._creds, self.ss_key, ws_name, source=self.SOURCE)

  def GetRowCacheByCol(self, ws_name, key):
    """Fetch the row cache with id=|key|."""
    self._Connect(ws_name)
    ss_key = gdata_lib.PrepColNameForSS(key)
    return self._scomm.GetRowCacheByCol(ss_key)

  def _EnsureColumnsExist(self, data_columns):
    """Ensures that |data_columns| exist in current spreadsheet worksheet.

    Assumes spreadsheet worksheet is already connected.

    Raises:
      SpreadsheetError if any column in |data_columns| is missing from
      the spreadsheet's current worksheet.
    """
    ss_cols = self._scomm.GetColumns()

    # Make sure all columns in data_table are supported in spreadsheet.
    missing_cols = [c for c in data_columns
                    if gdata_lib.PrepColNameForSS(c) not in ss_cols]
    if missing_cols:
      raise SpreadsheetError('Spreadsheet missing column(s): %s' %
                             ', '.join(missing_cols))

  def UploadColumnToWorksheet(self, ws_name, colIx, data):
    """Upload list |data| to column number |colIx| in worksheet |ws_name|.

    This will overwrite any existing data in that column.
    """
    self._Connect(ws_name)
    self._scomm.WriteColumnToWorksheet(colIx, data)

  def UploadSequentialRows(self, ws_name, data_table):
    """Upload |data_table| to the |ws_name| worksheet of sheet at self.ss_key.

    Data will be uploaded row-by-row in ascending ID_COL order. Missing
    values of ID_COL will be filled in by filler rows.

    Args:
      ws_name: Worksheet name for identifying worksheet within spreadsheet.
      data_table: table.Table object with rows to upload to worksheet.
    """
    self._Connect(ws_name)
    cros_build_lib.Info('Uploading stats rows to worksheet "%s" of spreadsheet'
                        ' "%s" now.', self._scomm.ws_name, self._scomm.ss_key)

    cros_build_lib.Debug('Getting cache of current spreadsheet contents.')
    id_col = data_table.ID_COL
    ss_id_col = gdata_lib.PrepColNameForSS(id_col)
    ss_row_cache = self._scomm.GetRowCacheByCol(ss_id_col)

    self._EnsureColumnsExist(data_table.GetColumns())

    # First see if a build_number is being skipped.  Allow the data_table to
    # add default (filler) rows if it wants to.  These rows may represent
    # aborted runs, for example.  ID_COL is assumed to hold integers.
    first_id_val = int(data_table[-1][id_col])
    prev_id_val = first_id_val - 1
    while str(prev_id_val) not in ss_row_cache:
      data_table.AppendGapRow(prev_id_val)
      prev_id_val -= 1
      # Sanity check that we have not created an infinite loop.
      assert prev_id_val >= 0

    # Spreadsheet is organized with oldest runs at the top and newest runs
    # at the bottom (because there is no interface for inserting new rows at
    # the top).  This is the reverse of data_table, so start at the end.
    for row in data_table[::-1]:
      row_dict = dict((gdata_lib.PrepColNameForSS(key), row[key])
                      for key in row)

      # See if row with the same id_col value already exists.
      id_val = row[id_col]
      ss_row = ss_row_cache.get(id_val)

      try:
        if ss_row:
          # Row already exists in spreadsheet.  See if contents any different.
          # Create dict representing values in row_dict different from ss_row.
          row_delta = dict((k, v) for k, v in row_dict.iteritems()
                           if not self._ValsEqual(v, ss_row[k]))
          if row_delta:
            cros_build_lib.Debug('Updating existing spreadsheet row for %s %s.',
                                 id_col, id_val)
            self._scomm.UpdateRowCellByCell(ss_row.ss_row_num, row_delta)
          else:
            cros_build_lib.Debug('Unchanged existing spreadsheet row for'
                                 ' %s %s.', id_col, id_val)
        else:
          cros_build_lib.Debug('Adding spreadsheet row for %s %s.',
                               id_col, id_val)
          self._scomm.InsertRow(row_dict)
      except gdata_lib.SpreadsheetError as e:
        cros_build_lib.Error('Failure while uploading spreadsheet row for'
                             ' %s %s with data %s. Error: %s.', id_col, id_val,
                             row_dict, e)


class StatsManager(object):
  """Abstract class for managing stats for one config target.

  Subclasses should specify the config target by passing them in to __init__.

  This class handles the following duties:
    1) Read a bunch of metadata.json URLs for the config target that are
       are no older than the given start date.
    2) Upload data to a Google Sheet, if specified by the subclass.
    3) Upload data to Graphite, if specified by the subclass.
  """
  # Subclasses can overwrite any of these.
  TABLE_CLASS = None
  UPLOAD_ROW_PER_BUILD = False
  # To be overridden by subclass. A dictionary mapping a |key| from
  # self.summary to (ws_name, colIx) tuples from the spreadsheet which
  # should be overwritten with the data from the self.summary[key]
  SUMMARY_SPREADSHEET_COLUMNS = {}
  CARBON_FUNCS_BY_VERSION = None
  BOT_TYPE = None

  # Whether to grab a count of what data has been written to sheets before.
  # This is needed if you are writing data to the Google Sheets spreadsheet.
  GET_SHEETS_VERSION = True

  def __init__(self, config_target, ss_key=None,
               no_sheets_version_filter=False):
    self.builds = []
    self.gs_ctx = gs.GSContext()
    self.config_target = config_target
    self.ss_key = ss_key
    self.no_sheets_version_filter = no_sheets_version_filter
    self.summary = {}
    self.actions = None


  # pylint: disable=W0613
  def Gather(self, start_date, end_date, sort_by_build_number=True,
             starting_build_number=0, creds=None):
    """Fetches build data into self.builds.

    Args:
      start_date: A datetime.date instance for the earliest build to
                  examine.
      end_date: A datetime.date instance for the latest build to
                examine.
      sort_by_build_number: Optional boolean. If True, builds will be
                            sorted by build number.
      starting_build_number: The lowest build number to include in
                             self.builds.
      creds: Login credentials as returned by PrepareCreds. (optional)
    """
    self.builds = self._FetchBuildData(start_date, end_date, self.config_target,
                                       self.gs_ctx)

    if sort_by_build_number:
      # Sort runs by build_number, from newest to oldest.
      cros_build_lib.Info('Sorting by build number now.')
      self.builds = sorted(self.builds, key=lambda b: b.build_number,
                           reverse=True)
    if starting_build_number:
      cros_build_lib.Info('Filtering to include builds after %s (inclusive).',
                          starting_build_number)
      self.builds = [b for b in self.builds
                     if b.build_number >= starting_build_number]

  @classmethod
  def _FetchBuildData(cls, start_date, end_date, config_target, gs_ctx):
    """Fetches BuildData for builds of |config_target| since |start_date|.

    Args:
      start_date: A datetime.date instance.
      end_date: A datetime.date instance for the latest build to
                examine.
      config_target: String config name to fetch metadata for.
      gs_ctx: A gs.GSContext instance.

    Returns:
      A list of of metadata_lib.BuildData objects that were fetched.
    """
    cros_build_lib.Info('Gathering data for %s from %s until %s',
                        config_target, start_date, end_date)
    urls = metadata_lib.GetMetadataURLsSince(config_target,
                                             start_date,
                                             end_date)
    cros_build_lib.Info('Found %d metadata.json URLs to process.\n'
                        '  From: %s\n  To  : %s', len(urls), urls[0], urls[-1])

    builds = metadata_lib.BuildData.ReadMetadataURLs(
        urls, gs_ctx, get_sheets_version=cls.GET_SHEETS_VERSION)
    cros_build_lib.Info('Read %d total metadata files.', len(builds))
    return builds

  # TODO(akeshet): Return statistics in dictionary rather than just printing
  # them.
  def Summarize(self):
    """Process and print a summary of statistics.

    Returns:
      An empty dictionary. Note: subclasses can extend this method and return
      non-empty dictionaries, with summarized statistics.
    """
    if self.builds:
      cros_build_lib.Info('%d total runs included, from build %d to %d.',
                          len(self.builds), self.builds[-1].build_number,
                          self.builds[0].build_number)
      total_passed = len([b for b in self.builds if b.Passed()])
      cros_build_lib.Info('%d of %d runs passed.', total_passed,
                          len(self.builds))
    else:
      cros_build_lib.Info('No runs included.')
    return {}

  @property
  def sheets_version(self):
    if self.TABLE_CLASS:
      return self.TABLE_CLASS.SHEETS_VERSION

    return -1

  @property
  def carbon_version(self):
    if self.CARBON_FUNCS_BY_VERSION:
      return len(self.CARBON_FUNCS_BY_VERSION) - 1

    return -1

  def UploadToSheet(self, creds):
    assert creds

    if self.UPLOAD_ROW_PER_BUILD:
      self._UploadBuildsToSheet(creds)

    if self.SUMMARY_SPREADSHEET_COLUMNS:
      self._UploadSummaryColumns(creds)

  def _UploadBuildsToSheet(self, creds):
    """Upload row-per-build data to adsheet."""
    if not self.TABLE_CLASS:
      cros_build_lib.Debug('No Spreadsheet uploading configured for %s.',
                           self.config_target)
      return

    # Filter for builds that need to send data to Sheets (unless overridden
    # by command line flag.
    if self.no_sheets_version_filter:
      builds = self.builds
    else:
      version = self.sheets_version
      builds = [b for b in self.builds if b.sheets_version < version]
      cros_build_lib.Info('Found %d builds that need to send Sheets v%d data.',
                          len(builds), version)

    if builds:
      # Fill a data table of type table_class from self.builds.
      # pylint: disable=E1102
      data_table = self.TABLE_CLASS()
      for build in builds:
        try:
          data_table.AppendBuildRow(build)
        except Exception:
          cros_build_lib.Error('Failed to add row for builder_number %s to'
                               ' table.  It came from %s.', build.build_number,
                               build.metadata_url)
          raise

      # Upload data table to sheet.
      uploader = SSUploader(creds, self.ss_key)
      uploader.UploadSequentialRows(data_table.WORKSHEET_NAME, data_table)

  def _UploadSummaryColumns(self, creds):
    """Overwrite summary columns in spreadsheet with appropriate data."""
    # Upload data table to sheet.
    uploader = SSUploader(creds, self.ss_key)
    for key, (ws_name, colIx) in self.SUMMARY_SPREADSHEET_COLUMNS.iteritems():
      uploader.UploadColumnToWorksheet(ws_name, colIx, self.summary[key])


  def SendToCarbon(self):
    if self.CARBON_FUNCS_BY_VERSION:
      for version, func in enumerate(self.CARBON_FUNCS_BY_VERSION):
        # Filter for builds that need to send data to Graphite.
        builds = [b for b in self.builds if b.carbon_version < version]
        cros_build_lib.Info('Found %d builds that need to send Graphite v%d'
                            ' data.', len(builds), version)
        if builds:
          func(self, builds)

  def MarkGathered(self):
    """Mark each metadata.json in self.builds as processed.

    Applies only to StatsManager subclasses that have UPLOAD_ROW_PER_BUILD
    True, as these do not want data from a given build to be re-uploaded.
    """
    if self.UPLOAD_ROW_PER_BUILD:
      metadata_lib.BuildData.MarkBuildsGathered(self.builds,
                                                self.sheets_version,
                                                self.carbon_version,
                                                gs_ctx=self.gs_ctx)


# TODO(mtennant): This class is an untested placeholder.
class CQSlaveStats(StatsManager):
  """Stats manager for all CQ slaves."""
  # TODO(mtennant): Add Sheets support for each CQ slave.
  TABLE_CLASS = None
  GET_SHEETS_VERSION = True

  def __init__(self, slave_target, **kwargs):
    super(CQSlaveStats, self).__init__(slave_target, **kwargs)

  # TODO(mtennant): This is totally untested, but is a refactoring of the
  # graphite code that was in place before for CQ slaves.
  def _SendToCarbonV0(self, builds):
    # Send runtime data.
    def _GetGraphName(build):
      bot_id = build['bot-config'].replace('-', '.')
      return 'buildbot.builders.%s.duration_seconds' % bot_id

    _SendToCarbon(builds, (
        _GetGraphName,
        lambda b: b.runtime_seconds,
        lambda b: b.epoch_time_seconds,
    ))


class CQMasterStats(StatsManager):
  """Manager stats gathering for the Commit Queue Master."""
  TABLE_CLASS = CQMasterTable
  UPLOAD_ROW_PER_BUILD = True
  BOT_TYPE = CQ
  GET_SHEETS_VERSION = True

  def __init__(self, **kwargs):
    super(CQMasterStats, self).__init__(CQ_MASTER, **kwargs)

  def _SendToCarbonV0(self, builds):
    # Send runtime data.
    _SendToCarbon(builds, (
        lambda b: 'buildbot.cq.run_time_seconds',
        lambda b: b.runtime_seconds,
        lambda b: b.epoch_time_seconds,
    ))

    # Send CLs per run data.
    _SendToCarbon(builds, (
        lambda b: 'buildbot.cq.cls_per_run',
        lambda b: b.count_changes,
        lambda b: b.epoch_time_seconds,
    ))

  # Organized by by increasing graphite version numbers, starting at 0.
  CARBON_FUNCS_BY_VERSION = (
      _SendToCarbonV0,
  )


class PFQMasterStats(StatsManager):
  """Manager stats gathering for the PFQ Master."""
  TABLE_CLASS = PFQMasterTable
  UPLOAD_ROW_PER_BUILD = True
  BOT_TYPE = PFQ
  GET_SHEETS_VERSION = True

  def __init__(self, **kwargs):
    super(PFQMasterStats, self).__init__(PFQ_MASTER, **kwargs)


class CanaryMasterStats(StatsManager):
  """Manager stats gathering for the Canary Master."""
  TABLE_CLASS = CanaryMasterTable
  UPLOAD_ROW_PER_BUILD = True
  BOT_TYPE = CANARY
  GET_SHEETS_VERSION = True

  def __init__(self, **kwargs):
    super(CanaryMasterStats, self).__init__(CANARY_MASTER, **kwargs)


# TODO(mtennant): Add token file support.  See upload_package_status.py.
def PrepareCreds(email, password=None):
  """Return a gdata_lib.Creds object from given credentials.

  Args:
    email: Email address.
    password: Password string.  If not specified then a password
      prompt will be used.

  Returns:
    A gdata_lib.Creds object.
  """
  creds = gdata_lib.Creds()
  creds.SetCreds(email, password)
  return creds


def _CheckOptions(options):
  # Ensure that specified start date is in the past.
  now = datetime.datetime.now()
  if options.start_date and now.date() < options.start_date:
    cros_build_lib.Error('Specified start date is in the future: %s',
                         options.start_date)
    return False

  # The --save option requires --email.
  if options.save and not options.email:
    cros_build_lib.Error('You must specify --email with --save.')
    return False

  return True


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  # Put options that control the mode of script into mutually exclusive group.
  mode = parser.add_mutually_exclusive_group(required=True)
  mode.add_argument('--cq-master', action='store_true', default=False,
                    help='Gather stats for the CQ master.')
  mode.add_argument('--pfq-master', action='store_true', default=False,
                    help='Gather stats for the PFQ master.')
  mode.add_argument('--canary-master', action='store_true', default=False,
                    help='Gather stats for the Canary master.')
  mode.add_argument('--cq-slaves', action='store_true', default=False,
                    help='Gather stats for all CQ slaves.')
  # TODO(mtennant): Other modes as they make sense, like maybe --release.

  mode = parser.add_mutually_exclusive_group(required=True)
  mode.add_argument('--start-date', action='store', type='date', default=None,
                    help='Limit scope to a start date in the past.')
  mode.add_argument('--past-month', action='store_true', default=False,
                    help='Limit scope to the past 30 days up to now.')
  mode.add_argument('--past-week', action='store_true', default=False,
                    help='Limit scope to the past week up to now.')
  mode.add_argument('--past-day', action='store_true', default=False,
                    help='Limit scope to the past day up to now.')

  parser.add_argument('--starting-build', action='store', type=int, default=0,
                      help='Filter to builds after given number (inclusive).')
  parser.add_argument('--end-date', action='store', type='date', default=None,
                      help='Limit scope to an end date in the past.')
  parser.add_argument('--save', action='store_true', default=False,
                      help='Save results to DB, if applicable.')
  parser.add_argument('--email', action='store', type=str, default=None,
                      help='Specify email for Google Sheets account to use.')

  mode = parser.add_argument_group('Advanced (use at own risk)')
  mode.add_argument('--no-upload', action='store_false', default=True,
                    dest='upload',
                    help='Skip uploading results to spreadsheet')
  mode.add_argument('--no-carbon', action='store_false', default=True,
                    dest='carbon',
                    help='Skip sending results to carbon/graphite')
  mode.add_argument('--no-mark-gathered', action='store_false', default=True,
                    dest='mark_gathered',
                    help='Skip marking results as gathered.')
  mode.add_argument('--no-sheets-version-filter', action='store_true',
                    default=False,
                    help='Upload all parsed metadata to spreasheet regardless '
                         'of sheets version.')
  mode.add_argument('--override-ss-key', action='store', default=None,
                    dest='ss_key',
                    help='Override spreadsheet key.')

  return parser


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)

  if not _CheckOptions(options):
    sys.exit(1)

  if options.end_date:
    end_date = options.end_date
  else:
    end_date = datetime.datetime.now().date()

  # Determine the start date to use, which is required.
  if options.start_date:
    start_date = options.start_date
  else:
    assert options.past_month or options.past_week or options.past_day
    if options.past_month:
      start_date = end_date - datetime.timedelta(days=30)
    elif options.past_week:
      start_date = end_date - datetime.timedelta(days=7)
    else:
      start_date = end_date - datetime.timedelta(days=1)

  # Prepare the rounds of stats gathering to do.
  stats_managers = []

  if options.cq_master:
    stats_managers.append(
        CQMasterStats(
            ss_key=options.ss_key or CQ_SS_KEY,
            no_sheets_version_filter=options.no_sheets_version_filter))

  if options.pfq_master:
    stats_managers.append(
        PFQMasterStats(
            ss_key=options.ss_key or PFQ_SS_KEY,
            no_sheets_version_filter=options.no_sheets_version_filter))

  if options.canary_master:
    stats_managers.append(
        CanaryMasterStats(
            ss_key=options.ss_key or CANARY_SS_KEY,
            no_sheets_version_filter=options.no_sheets_version_filter))

  if options.cq_slaves:
    targets = _GetSlavesOfMaster(CQ_MASTER)
    for target in targets:
      # TODO(mtennant): Add spreadsheet and/or graphite support for cq-slaves.
      stats_managers.append(CQSlaveStats(target))

  # If options.save is set and any of the instructions include a table class,
  # or specify summary columns for upload, prepare spreadsheet creds object
  # early.
  creds = None
  if options.save and any((stats.UPLOAD_ROW_PER_BUILD or
                           stats.SUMMARY_SPREADSHEET_COLUMNS)
                          for stats in stats_managers):
    # TODO(mtennant): See if this can work with two-factor authentication.
    # TODO(mtennant): Eventually, we probably want to use 90-day certs to
    # run this as a cronjob on a ganeti instance.
    creds = PrepareCreds(options.email)

  # Now run through all the stats gathering that is requested.
  for stats_mgr in stats_managers:
    stats_mgr.Gather(start_date, end_date,
                     starting_build_number=options.starting_build,
                     creds=creds)
    stats_mgr.Summarize()

    if options.save:
      # Send data to spreadsheet, if applicable.
      if options.upload:
        stats_mgr.UploadToSheet(creds)

      # Send data to Carbon/Graphite, if applicable.
      if options.carbon:
        stats_mgr.SendToCarbon()

      # Mark these metadata.json files as processed.
      if options.mark_gathered:
        stats_mgr.MarkGathered()

    cros_build_lib.Info('Finished with %s.\n\n', stats_mgr.config_target)


# Background: This function logs the number of tryjob runs, both internal
# and external, to Graphite.  It gets the data from git logs.  It was in
# place, in a very different form, before the migration to
# gather_builder_stats.  It is simplified here, but entirely untested and
# not plumbed into gather_builder_stats anywhere.
def GraphiteTryJobInfoUpToNow(internal, start_date, end_date):
  """Find the amount of tryjobs that finished on a particular day.

  Args:
    internal: If true report for internal, if false report external.
    start_date: datetime.date object for date to start on.
    end_date: datetime.date object for date to end on.
  """
  carbon_lines = []

  # Apparently scottz had 'trybot' and 'trybot-internal' checkouts in
  # his home directory which this code relied on.  Any new solution that
  # also relies on git logs will need a place to look for them.
  if internal:
    repo_path = '/some/path/to/trybot-internal'
    marker = 'internal'
  else:
    repo_path = '/some/path/to/trybot'
    marker = 'external'

  # Make sure the trybot checkout is up to date.
  os.chdir(repo_path)
  cros_build_lib.RunCommand(['git', 'pull'], cwd=repo_path)

  # Now get a list of datetime objects, in hourly deltas.
  start = datetime.datetime(start_date.year, start_date.month, start_date.day)
  hour_delta = datetime.timedelta(hours=1)
  end = start + hour_delta
  while end < end_date:
    git_cmd = ['git', 'log', '--since="%s"' % start,
               '--until="%s"' % end, '--name-only', '--pretty=format:']
    result = cros_build_lib.RunCommand(git_cmd, cwd=repo_path)

    # Expect one line per tryjob run in the specified hour.
    count = len(l for l in result.output.splitlines() if l.strip())

    carbon_lines.append('buildbot.tryjobs.%s.hourly %s %s' %
                        (marker, count, (end - EPOCH_START).total_seconds()))

  graphite.SendToCarbon(carbon_lines)
