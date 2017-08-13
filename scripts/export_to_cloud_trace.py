# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Export spans to Cloud Trace."""
from __future__ import print_function

import errno
import itertools
import json
import os
import pprint
import time

from googleapiclient import discovery
import inotify_simple
from oauth2client.client import GoogleCredentials

from chromite.lib import commandline
from chromite.lib import cros_logging as log


BATCH_PATIENCE = 10 * 60
MIN_BATCH_SIZE = 300
SPAN_LOG_DIR = '/var/log/trace'
CREDS_PATH = '/creds/service_accounts/service-account-trace.json'
SCOPES = ['https://www.googleapis.com/auth/trace.append']


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--service-acct-json', type=str, action='store',
                      default=CREDS_PATH,
                      help='Path to service account credentials JSON file.')
  parser.add_argument('--project-id', '-i', type=str, action='store',
                      default=None,
                      help=('Optional project_id of datastore to write to. If '
                            'not supplied, will be taken from credentials '
                            'file.'))
  return parser


class _DirWatcher(object):
  """Watches a directory with inotify, and parses JSON blobs.

  Any time a file in the watched directory is modified, it reads it
  line-by-line and yields JSON blobs from it.
  """
  # Only wait 50ms - we may want to send a smaller batch if it takes too long
  # for the next spans to arrive.
  READ_TIMEOUT_MS = 50
  # Accumulate 50ms of events before reading from the inotify filehandle.
  READ_DELAY_MS = 50

  def __init__(self, directory, flags=inotify_simple.flags.CLOSE_WRITE):
    self._inotify = inotify_simple.INotify()
    self._inotify_flags = flags
    self._dir = directory

  def _AddWatch(self):
    """Adds the inotify watch to the directory."""
    self._inotify.add_watch(self._dir, self._inotify_flags)

  def _FileChanges(self):
    """Returns batches of file events as they occur."""
    while True:
      events = self._inotify.read(timeout=self.READ_TIMEOUT_MS,
                                  read_delay=self.READ_DELAY_MS)
      yield events

  def __enter__(self):
    """Enters the context. Closes the inotify watcher on exit."""
    self._AddWatch()
    return self

  def __exit__(self, _type, _value, _traceback):
    """Closes the inotify watcher upon context exit."""
    self._inotify.close()

  def Batches(self):
    """Yields a stream of file line batches found using inotify."""
    for event_batch in self._FileChanges():
      # Immediately read the batch and clean it up, otherwise it may be
      # overwritten by a subsequent process with the same PID.
      files = [os.path.join(self._dir, e.name) for e in event_batch]
      lines = tuple(_ReadBatch(files))
      _CleanupBatch(files)
      if files:
        log.debug("Found changes in %s", pprint.pformat(files))
      yield lines


def _ReadBatch(file_batch):
  """Read each line in a list of files as json.

  Args:
    file_batch: A list of file paths to read.
  """
  for f in file_batch:
    with open(f) as fp:
      for line in fp:
        yield line


def _CleanupBatch(files):
  """Remove each file in a list of files, warning if they don't exist.

  Args:
    files: A list of file paths to remove.
  """
  for path in files:
    try:
      os.remove(path)
    except OSError as error:
      if error.errno == errno.ENOENT:
        log.exception(
            'warning: could not find %s while attempting to remove it.', path)
      else:
        raise


def _MapIgnoringErrors(f, sequence, exception_type=Exception):
  """Maps a function over a stream ignoring exceptions.

  Args:
    f: A function to call.
    sequence: An iterable to map over, forgiving exceptions
    exception_type: The specific exception to forgive.
  """
  for item in sequence:
    try:
      yield f(item)
    except exception_type as e:
      log.exception('Ignoring error while mapping: %s.', e)


def _ImpatientlyRebatched(batch_sequence, ideal_size, patience):
  """Makes large batches from a stream of batches, with a maximum patience.

  Args:
    batch_sequence: An iterable of batches to create larger batches from.
    ideal_size: An ideal minimum number of entries per batch.
    patience: A maximum number of seconds to wait before sending a batch.

  Yields:
    Lists of entries from |stream| whose len() is at least |batch_size|.
    If |patience| seconds elapse before the |batch_size| is reached,
    the incomplete batch is yielded as-is (possibly empty).
  """
  # TODO(phobbs) this is probably easier to accomplish with rxpy.
  while True:
    start_time = time.time()
    accum = []

    for batch in batch_sequence:
      accum.extend(batch)
      if time.time() - start_time > patience:
        break
      if len(accum) >= ideal_size:
        break

    yield accum


def _GroupBy(iterable, key):
  """Groups an unsorted iterable by a key.

  Args:
    iterable: An unsorted iterable to group.
    key: A key to group by.
  """
  items = sorted(iterable, key=key)
  for k, group in itertools.groupby(items, key=key):
    yield k, list(group)


def _BatchAndSendSpans(project_id, client, batch_sequence):
  """Batches and sends spans to the cloud trace API.

  Args:
    project_id: The Google Cloud project id
    client: The google python api client
    batch_sequence: An iterable of Span batches represented as JSON objects.
  """
  for batch in _ImpatientlyRebatched(batch_sequence, MIN_BATCH_SIZE,
                                     BATCH_PATIENCE):
    traces = []
    groups = _GroupBy(batch, key=lambda span: span.get('traceId'))
    for trace_id, spans in groups:
      traces.append({
          'traceId': trace_id,
          'projectId': project_id,
          'spans': spans
      })
    if traces:
      client.projects().patchTraces(
          projectId=project_id, body={'traces': traces})


def _ReadAndDeletePreexisting(log_dir):
  """Reads pre-existing log files in |log_dir| and cleans them up.

  Args:
    log_dir: The directory to read from.
  """
  preexisting_files = [os.path.join(log_dir, f)
                       for f in os.listdir(SPAN_LOG_DIR)]
  log.info("Processing pre-existing logs: %s",
           pprint.pformat(preexisting_files))
  preexisting_lines = tuple(_ReadBatch(preexisting_files))
  _CleanupBatch(preexisting_files)
  return preexisting_lines


def _WatchAndSendSpans(project_id, client):
  """Watches a directory and sends batches of spans.

  Args:
    project_id: The Google Cloud project id
    client: The google python api client
  """
  with _DirWatcher(SPAN_LOG_DIR) as watcher:
    preexisting_lines = _ReadAndDeletePreexisting(SPAN_LOG_DIR)

    new_batches = watcher.Batches()
    all_batches = itertools.chain([preexisting_lines], new_batches)

    # Ignore lines that don't parse.
    batches = itertools.ifilter(None, (
        _MapIgnoringErrors(
            json.loads,
            batch,
            exception_type=ValueError)
        for batch in all_batches))

    # Rebatch the lines.
    _BatchAndSendSpans(project_id, client, batches)

#-- Code for talking to the trace API. -----------------------------------------
def _MakeCreds(creds_path):
  """Creates a GoogleCredentials object with the trace.append scope.

  Args:
    creds_path: Path to the credentials file to use.
  """
  return GoogleCredentials.from_stream(
      os.path.expanduser(creds_path)
  ).create_scoped(SCOPES)


def _Client(creds_path):
  """Returns a Cloud Trace API client object."""
  return discovery.build('cloudtrace', 'v1', credentials=_MakeCreds(creds_path))


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)

  creds_file = options.service_acct_json
  project_id = options.project_id
  client = _Client(creds_path=creds_file)

  _WatchAndSendSpans(project_id, client)
