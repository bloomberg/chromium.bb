# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Descriptive metrics for Clovis.

When executed as a script, shows a graph of the amount of data to download for
a new visit to the same page, with a given time interval.
"""


from request_track import CachingPolicy


def _RequestTransferSize(request):
  def HeadersSize(headers):
    # 4: ':', ' ', '\r', '\n'
    return sum(len(k) + len(v) + 4 for (k, v) in headers.items())
  if request.protocol == 'data':
    return {'get': 0, 'request_headers': 0, 'response_headers': 0, 'body': 0}
  return {'get': len('GET ') + len(request.url) + 2,
          'request_headers': HeadersSize(request.request_headers or {}),
          'response_headers': HeadersSize(request.response_headers or {}),
          'body': request.encoded_data_length}


def TotalTransferSize(trace):
  """Returns the total transfer size (uploaded, downloaded) from a trace.

  This is an estimate as we assume:
  - 200s (for the size computation)
  - GET only.
  """
  uploaded_bytes = 0
  downloaded_bytes = 0
  for request in trace.request_track.GetEvents():
    request_bytes = _RequestTransferSize(request)
    uploaded_bytes += request_bytes['get'] + request_bytes['request_headers']
    downloaded_bytes += (len('HTTP/1.1 200 OK')
                         + request_bytes['response_headers']
                         + request_bytes['body'])
  return (uploaded_bytes, downloaded_bytes)


def TransferredDataRevisit(trace, after_time_s, assume_validation_ok=False):
  """Returns the amount of data transferred for a revisit.

  Args:
    trace: (LoadingTrace) loading trace.
    after_time_s: (float) Time in s after which the site is revisited.
    assume_validation_ok: (bool) Assumes that the resources to validate return
                          304s.

  Returns:
    (uploaded_bytes, downloaded_bytes)
  """
  uploaded_bytes = 0
  downloaded_bytes = 0
  for request in trace.request_track.GetEvents():
    caching_policy = CachingPolicy(request)
    policy = caching_policy.PolicyAtDate(request.wall_time + after_time_s)
    request_bytes = _RequestTransferSize(request)
    if policy == CachingPolicy.VALIDATION_NONE:
      continue
    uploaded_bytes += request_bytes['get'] + request_bytes['request_headers']
    if (policy in (CachingPolicy.VALIDATION_SYNC,
                   CachingPolicy.VALIDATION_ASYNC)
        and caching_policy.HasValidators() and assume_validation_ok):
      downloaded_bytes += len('HTTP/1.1 304 NOT MODIFIED\r\n')
      continue
    downloaded_bytes += (len('HTTP/1.1 200 OK\r\n')
                         + request_bytes['response_headers']
                         + request_bytes['body'])
  return (uploaded_bytes, downloaded_bytes)


def PlotTransferSizeVsTimeBetweenVisits(trace):
  times = [10, 60, 300, 600, 3600, 4 * 3600, 12 * 3600, 24 * 3600]
  labels = ['10s', '1m', '10m', '1h', '4h', '12h', '1d']
  (_, total_downloaded) = TotalTransferSize(trace)
  downloaded = [TransferredDataRevisit(trace, delta_t)[1] for delta_t in times]
  plt.figure()
  plt.title('Amount of data to download for a revisit - %s' % trace.url)
  plt.xlabel('Time between visits (log)')
  plt.ylabel('Amount of data (bytes)')
  plt.plot(times, downloaded, 'k+--')
  plt.axhline(total_downloaded, color='k', linewidth=2)
  plt.xscale('log')
  plt.xticks(times, labels)
  plt.show()


def main(trace_filename):
  trace = loading_trace.LoadingTrace.FromJsonFile(trace_filename)
  PlotTransferSizeVsTimeBetweenVisits(trace)


if __name__ == '__main__':
  import sys
  from matplotlib import pylab as plt
  import loading_trace

  main(sys.argv[1])
