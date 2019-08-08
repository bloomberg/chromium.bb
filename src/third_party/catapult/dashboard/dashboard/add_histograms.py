# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""URL endpoint for adding new histograms to the dashboard."""
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import

import cloudstorage
import decimal
import ijson
import json
import logging
import sys
import uuid
import zlib

from google.appengine.api import taskqueue

from dashboard.api import api_request_handler
from dashboard.common import datastore_hooks
from dashboard.common import histogram_helpers
from dashboard.common import request_handler
from dashboard.common import timing
from dashboard.common import utils
from dashboard.models import graph_data
from dashboard.models import histogram
from tracing.value import histogram_set
from tracing.value.diagnostics import diagnostic
from tracing.value.diagnostics import reserved_infos

SUITE_LEVEL_SPARSE_DIAGNOSTIC_NAMES = set([
    reserved_infos.ARCHITECTURES.name,
    reserved_infos.BENCHMARKS.name,
    reserved_infos.BENCHMARK_DESCRIPTIONS.name,
    reserved_infos.BOTS.name,
    reserved_infos.BUG_COMPONENTS.name,
    reserved_infos.DOCUMENTATION_URLS.name,
    reserved_infos.GPUS.name,
    reserved_infos.MASTERS.name,
    reserved_infos.MEMORY_AMOUNTS.name,
    reserved_infos.OS_NAMES.name,
    reserved_infos.OS_VERSIONS.name,
    reserved_infos.OWNERS.name,
    reserved_infos.PRODUCT_VERSIONS.name,
])

HISTOGRAM_LEVEL_SPARSE_DIAGNOSTIC_NAMES = set([
    reserved_infos.DEVICE_IDS.name,
    reserved_infos.STORIES.name,
    reserved_infos.STORYSET_REPEATS.name,
    reserved_infos.STORY_TAGS.name,
])

SPARSE_DIAGNOSTIC_NAMES = SUITE_LEVEL_SPARSE_DIAGNOSTIC_NAMES.union(
    HISTOGRAM_LEVEL_SPARSE_DIAGNOSTIC_NAMES)


TASK_QUEUE_NAME = 'histograms-queue'

_RETRY_PARAMS = cloudstorage.RetryParams(backoff_factor=1.1)
_TASK_RETRY_LIMIT = 4
_ZLIB_BUFFER_SIZE = 4096


def _CheckRequest(condition, msg):
  if not condition:
    raise api_request_handler.BadRequestError(msg)


class DecompressFileWrapper(object):
  """A file-like object implementing inline decompression.

  This class wraps a file-like object and does chunk-based decoding of the data.
  We only implement the read() function supporting fixed-chunk reading, capped
  to a predefined constant buffer length.

  Example

    with open('filename', 'r') as input:
      decompressor = DecompressFileWrapper(input)
      while True:
        chunk = decompressor.read(4096)
        if len(chunk) == 0:
          break
        // handle the chunk with size <= 4096
  """

  def __init__(self, source_file, buffer_size=_ZLIB_BUFFER_SIZE):
    self.source_file = source_file
    self.decompressor = zlib.decompressobj()
    self.buffer_size = buffer_size

  def __enter__(self):
    return self

  def read(self, size=None): # pylint: disable=invalid-name
    if size is None or size < 0:
      size = self.buffer_size

    # We want to read chunks of data from the buffer, chunks at a time.
    temporary_buffer = self.decompressor.unconsumed_tail
    if len(temporary_buffer) < self.buffer_size / 2:
      raw_buffer = self.source_file.read(size)
      if raw_buffer != '':
        temporary_buffer += raw_buffer

    if len(temporary_buffer) == 0:
      return u''

    decompressed_data = self.decompressor.decompress(temporary_buffer, size)
    return decompressed_data

  def close(self): # pylint: disable=invalid-name
    self.decompressor.flush()

  def __exit__(self, exception_type, exception_value, execution_traceback):
    self.close()
    return False


def _LoadHistogramList(input_file):
  """Incremental file decoding and JSON parsing when handling new histograms.

  This helper function takes an input file which yields fragments of JSON
  encoded histograms then incrementally builds the list of histograms to return
  the fully formed list in the end.

  Returns
    This function returns an instance of a list() containing dict()s decoded
    from the input_file.

  Raises
    This function may raise ValueError instances if we end up not finding valid
    JSON fragments inside the file.
  """
  try:
    with timing.WallTimeLogger('json.load'):
      def NormalizeDecimals(obj):
        # Traverse every object in obj to turn Decimal objects into floats.
        if isinstance(obj, decimal.Decimal):
          return float(obj)
        if isinstance(obj, dict):
          for k, v in obj.items():
            obj[k] = NormalizeDecimals(v)
        if isinstance(obj, list):
          obj = [NormalizeDecimals(x) for x in obj]
        return obj

      objects = [NormalizeDecimals(x) for x in ijson.items(input_file, 'item')]

  except ijson.JSONError as e:
    # Wrap exception in a ValueError
    raise ValueError('Failed to parse JSON: %s' % (e))

  return objects


class AddHistogramsProcessHandler(request_handler.RequestHandler):

  def post(self):
    datastore_hooks.SetPrivilegedRequest()

    try:
      params = json.loads(self.request.body)
      gcs_file_path = params['gcs_file_path']

      try:
        gcs_file = cloudstorage.open(
            gcs_file_path, 'r', retry_params=_RETRY_PARAMS)
        with DecompressFileWrapper(gcs_file) as decompressing_file:
          histogram_dicts = _LoadHistogramList(decompressing_file)

        gcs_file.close()

        ProcessHistogramSet(histogram_dicts)
      finally:
        cloudstorage.delete(gcs_file_path, retry_params=_RETRY_PARAMS)

    except Exception as e: # pylint: disable=broad-except
      logging.error('Error processing histograms: %r', e.message)
      self.response.out.write(json.dumps({'error': e.message}))


class AddHistogramsHandler(api_request_handler.ApiRequestHandler):

  def _CheckUser(self):
    self._CheckIsInternalUser()

  def Post(self):
    if utils.IsDevAppserver():
      # Don't require developers to zip the body.
      # In prod, the data will be written to cloud storage and processed on the
      # taskqueue, so the caller will not see any errors. In dev_appserver,
      # process the data immediately so the caller will see errors.
      ProcessHistogramSet(json.loads(self.request.body))
      return

    with timing.WallTimeLogger('decompress'):
      try:
        data_str = self.request.body

        # Try to decompress at most 100 bytes from the data, only to determine
        # if we've been given compressed payload.
        zlib.decompressobj().decompress(data_str, 100)
        logging.info('Recieved compressed data.')
      except zlib.error:
        data_str = self.request.get('data')
        if not data_str:
          raise api_request_handler.BadRequestError(
              'Missing or uncompressed data.')
        data_str = zlib.compress(data_str)
        logging.info('Recieved uncompressed data.')

    if not data_str:
      raise api_request_handler.BadRequestError('Missing "data" parameter')

    filename = uuid.uuid4()
    params = {'gcs_file_path': '/add-histograms-cache/%s' % filename}

    gcs_file = cloudstorage.open(
        params['gcs_file_path'], 'w',
        content_type='application/octet-stream',
        retry_params=_RETRY_PARAMS)
    gcs_file.write(data_str)
    gcs_file.close()

    retry_options = taskqueue.TaskRetryOptions(
        task_retry_limit=_TASK_RETRY_LIMIT)
    queue = taskqueue.Queue('default')
    queue.add(
        taskqueue.Task(
            url='/add_histograms/process', payload=json.dumps(params),
            retry_options=retry_options))


def _LogDebugInfo(histograms):
  hist = histograms.GetFirstHistogram()
  if not hist:
    logging.info('No histograms in data.')
    return

  log_urls = hist.diagnostics.get(reserved_infos.LOG_URLS.name)
  if log_urls:
    log_urls = list(log_urls)
    msg = 'Buildbot URL: %s' % str(log_urls)
    logging.info(msg)
  else:
    logging.info('No LOG_URLS in data.')

  build_urls = hist.diagnostics.get(reserved_infos.BUILD_URLS.name)
  if build_urls:
    build_urls = list(build_urls)
    msg = 'Build URL: %s' % str(build_urls)
    logging.info(msg)
  else:
    logging.info('No BUILD_URLS in data.')


def ProcessHistogramSet(histogram_dicts):
  if not isinstance(histogram_dicts, list):
    raise api_request_handler.BadRequestError(
        'HistogramSet JSON much be a list of dicts')

  histograms = histogram_set.HistogramSet()

  with timing.WallTimeLogger('hs.ImportDicts'):
    histograms.ImportDicts(histogram_dicts)

  with timing.WallTimeLogger('hs.DeduplicateDiagnostics'):
    histograms.DeduplicateDiagnostics()

  if len(histograms) == 0:
    raise api_request_handler.BadRequestError(
        'HistogramSet JSON must contain at least one histogram.')

  with timing.WallTimeLogger('hs._LogDebugInfo'):
    _LogDebugInfo(histograms)

  with timing.WallTimeLogger('InlineDenseSharedDiagnostics'):
    InlineDenseSharedDiagnostics(histograms)

  # TODO(#4242): Get rid of this.
  # https://github.com/catapult-project/catapult/issues/4242
  with timing.WallTimeLogger('_PurgeHistogramBinData'):
    _PurgeHistogramBinData(histograms)

  with timing.WallTimeLogger('_GetDiagnosticValue calls'):
    master = _GetDiagnosticValue(
        reserved_infos.MASTERS.name, histograms.GetFirstHistogram())
    bot = _GetDiagnosticValue(
        reserved_infos.BOTS.name, histograms.GetFirstHistogram())
    benchmark = _GetDiagnosticValue(
        reserved_infos.BENCHMARKS.name, histograms.GetFirstHistogram())
    benchmark_description = _GetDiagnosticValue(
        reserved_infos.BENCHMARK_DESCRIPTIONS.name,
        histograms.GetFirstHistogram(), optional=True)

  with timing.WallTimeLogger('_ValidateMasterBotBenchmarkName'):
    _ValidateMasterBotBenchmarkName(master, bot, benchmark)

  with timing.WallTimeLogger('ComputeRevision'):
    suite_key = utils.TestKey('%s/%s/%s' % (master, bot, benchmark))
    logging.info('Suite: %s', suite_key.id())

    revision = ComputeRevision(histograms)
    logging.info('Revision: %s', revision)

    internal_only = graph_data.Bot.GetInternalOnlySync(master, bot)

  revision_record = histogram.HistogramRevisionRecord.GetOrCreate(
      suite_key, revision)
  revision_record.put()

  last_added = histogram.HistogramRevisionRecord.GetLatest(
      suite_key).get_result()

  # On first upload, a query immediately following a put may return nothing.
  if not last_added:
    last_added = revision_record

  _CheckRequest(last_added, 'No last revision')

  # We'll skip the histogram-level sparse diagnostics because we need to
  # handle those with the histograms, below, so that we can properly assign
  # test paths.
  with timing.WallTimeLogger('FindSuiteLevelSparseDiagnostics'):
    suite_level_sparse_diagnostic_entities = FindSuiteLevelSparseDiagnostics(
        histograms, suite_key, revision, internal_only)

  # TODO(896856): Refactor master/bot computation to happen above this line
  # so that we can replace with a DiagnosticRef rather than a full diagnostic.
  with timing.WallTimeLogger('DeduplicateAndPut'):
    new_guids_to_old_diagnostics = (
        histogram.SparseDiagnostic.FindOrInsertDiagnostics(
            suite_level_sparse_diagnostic_entities, suite_key,
            revision, last_added.revision).get_result())

  with timing.WallTimeLogger('ReplaceSharedDiagnostic calls'):
    for new_guid, old_diagnostic in new_guids_to_old_diagnostics.items():
      histograms.ReplaceSharedDiagnostic(
          new_guid, diagnostic.Diagnostic.FromDict(old_diagnostic))

  with timing.WallTimeLogger('_BatchHistogramsIntoTasks'):
    tasks = _BatchHistogramsIntoTasks(
        suite_key.id(), histograms, revision, benchmark_description)

  with timing.WallTimeLogger('_QueueHistogramTasks'):
    _QueueHistogramTasks(tasks)


def _ValidateMasterBotBenchmarkName(master, bot, benchmark):
  for n in (master, bot, benchmark):
    if '/' in n:
      raise api_request_handler.BadRequestError('Illegal slash in %s' % n)


def _QueueHistogramTasks(tasks):
  queue = taskqueue.Queue(TASK_QUEUE_NAME)
  futures = []
  for i in range(0, len(tasks), taskqueue.MAX_TASKS_PER_ADD):
    f = queue.add_async(tasks[i:i + taskqueue.MAX_TASKS_PER_ADD])
    futures.append(f)
  for f in futures:
    f.get_result()


def _MakeTask(params):
  return taskqueue.Task(
      url='/add_histograms_queue', payload=json.dumps(params),
      _size_check=False)


def _BatchHistogramsIntoTasks(
    suite_path, histograms, revision, benchmark_description):
  params = []
  tasks = []

  base_size = _MakeTask([]).size
  estimated_size = 0

  duplicate_check = set()

  for hist in histograms:
    diagnostics = FindHistogramLevelSparseDiagnostics(hist)

    # TODO(896856): Don't compute full diagnostics, because we need anyway to
    # call GetOrCreate here and in the queue.
    test_path = '%s/%s' % (suite_path, histogram_helpers.ComputeTestPath(hist))

    if test_path in duplicate_check:
      raise api_request_handler.BadRequestError(
          'Duplicate histogram detected: %s' % test_path)
    duplicate_check.add(test_path)

    # TODO(#4135): Batch these better than one per task.
    task_dict = _MakeTaskDict(
        hist, test_path, revision, benchmark_description, diagnostics)

    estimated_size_dict = len(json.dumps(task_dict))
    estimated_size += estimated_size_dict

    # Creating the task directly and getting the size back is slow, so we just
    # keep a running total of estimated task size. A bit hand-wavy but the #
    # of histograms per task doesn't need to be perfect, just has to be under
    # the max task size.
    estimated_total_size = estimated_size * 1.05 + base_size + 1024
    if estimated_total_size > taskqueue.MAX_TASK_SIZE_BYTES:
      t = _MakeTask(params)
      tasks.append(t)
      params = []
      estimated_size = estimated_size_dict

    params.append(task_dict)

  if params:
    t = _MakeTask(params)
    tasks.append(t)

  return tasks


def _MakeTaskDict(
    hist, test_path, revision, benchmark_description, diagnostics):
  # TODO(simonhatch): "revision" is common to all tasks, as is the majority of
  # the test path
  params = {
      'test_path': test_path,
      'revision': revision,
      'benchmark_description': benchmark_description
  }

  # By changing the GUID just before serializing the task, we're making it
  # unique for each histogram. This avoids each histogram trying to write the
  # same diagnostic out (datastore contention), at the cost of copyin the
  # data. These are sparsely written to datastore anyway, so the extra
  # storage should be minimal.
  for d in diagnostics.values():
    d.ResetGuid()

  diagnostics = {k: d.AsDict() for k, d in diagnostics.items()}

  params['diagnostics'] = diagnostics
  params['data'] = hist.AsDict()

  return params


def FindSuiteLevelSparseDiagnostics(
    histograms, suite_key, revision, internal_only):
  diagnostics = {}
  for hist in histograms:
    for name, diag in hist.diagnostics.items():
      if name in SUITE_LEVEL_SPARSE_DIAGNOSTIC_NAMES:
        existing_entity = diagnostics.get(name)
        if existing_entity is None:
          diagnostics[name] = histogram.SparseDiagnostic(
              id=diag.guid, data=diag.AsDict(), test=suite_key,
              start_revision=revision, end_revision=sys.maxsize, name=name,
              internal_only=internal_only)
        elif existing_entity.key.id() != diag.guid:
          raise ValueError(
              name + ' diagnostics must be the same for all histograms')
  return list(diagnostics.values())


def FindHistogramLevelSparseDiagnostics(hist):
  diagnostics = {}
  for name, diag in hist.diagnostics.items():
    if name in HISTOGRAM_LEVEL_SPARSE_DIAGNOSTIC_NAMES:
      diagnostics[name] = diag
  return diagnostics


def _GetDiagnosticValue(name, hist, optional=False):
  if optional:
    if name not in hist.diagnostics:
      return None

  _CheckRequest(
      name in hist.diagnostics,
      'Histogram [%s] missing "%s" diagnostic' % (hist.name, name))
  value = hist.diagnostics[name]
  _CheckRequest(
      len(value) == 1,
      'Histograms must have exactly 1 "%s"' % name)
  return value.GetOnlyElement()


def ComputeRevision(histograms):
  _CheckRequest(len(histograms) > 0, 'Must upload at least one histogram')
  rev = _GetDiagnosticValue(
      reserved_infos.POINT_ID.name,
      histograms.GetFirstHistogram(), optional=True)

  if rev is None:
    rev = _GetDiagnosticValue(
        reserved_infos.CHROMIUM_COMMIT_POSITIONS.name,
        histograms.GetFirstHistogram(), optional=True)

  if rev is None:
    revision_timestamps = histograms.GetFirstHistogram().diagnostics.get(
        reserved_infos.REVISION_TIMESTAMPS.name)
    _CheckRequest(revision_timestamps is not None,
                  'Must specify REVISION_TIMESTAMPS, CHROMIUM_COMMIT_POSITIONS,'
                  ' or POINT_ID')
    rev = revision_timestamps.max_timestamp

  if not isinstance(rev, int):
    raise api_request_handler.BadRequestError(
        'Point ID must be an integer.')

  return rev


def InlineDenseSharedDiagnostics(histograms):
  # TODO(896856): Delete inlined diagnostics from the set
  for hist in histograms:
    diagnostics = hist.diagnostics
    for name, diag in diagnostics.items():
      if name not in SPARSE_DIAGNOSTIC_NAMES:
        diag.Inline()


def _PurgeHistogramBinData(histograms):
  # We do this because RelatedEventSet and Breakdown data in bins is
  # enormous in their current implementation.
  for cur_hist in histograms:
    for cur_bin in cur_hist.bins:
      for dm in cur_bin.diagnostic_maps:
        keys = list(dm.keys())
        for k in keys:
          del dm[k]
