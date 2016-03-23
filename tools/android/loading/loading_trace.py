# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Represents the trace of a page load."""

import json

import devtools_monitor
import page_track
import request_track
import tracing


class LoadingTrace(object):
  """Represents the trace of a page load."""
  _URL_KEY = 'url'
  _METADATA_KEY = 'metadata'
  _PAGE_KEY = 'page_track'
  _REQUEST_KEY = 'request_track'
  _TRACING_KEY = 'tracing_track'

  def __init__(self, url, metadata, page, request, tracing_track):
    """Initializes a loading trace instance.

    Args:
      url: (str) URL that has been loaded
      metadata: (dict) Metadata associated with the load.
      page: (PageTrack) instance of PageTrack.
      request: (RequestTrack) instance of RequestTrack.
      tracing_track: (TracingTrack) instance of TracingTrack.
    """
    self.url = url
    self.metadata = metadata
    self.page_track = page
    self.request_track = request
    self.tracing_track = tracing_track

  def ToJsonDict(self):
    """Returns a dictionary representing this instance."""
    result = {self._URL_KEY: self.url, self._METADATA_KEY: self.metadata,
              self._PAGE_KEY: self.page_track.ToJsonDict(),
              self._REQUEST_KEY: self.request_track.ToJsonDict(),
              self._TRACING_KEY: self.tracing_track.ToJsonDict()}
    return result

  def ToJsonFile(self, json_path):
    """Save a json file representing this instance."""
    json_dict = self.ToJsonDict()
    with open(json_path, 'w') as output_file:
       json.dump(json_dict, output_file, indent=2)

  @classmethod
  def FromJsonDict(cls, json_dict):
    """Returns an instance from a dictionary returned by ToJsonDict()."""
    keys = (cls._URL_KEY, cls._METADATA_KEY, cls._PAGE_KEY, cls._REQUEST_KEY,
            cls._TRACING_KEY)
    assert all(key in json_dict for key in keys)
    page = page_track.PageTrack.FromJsonDict(json_dict[cls._PAGE_KEY])
    request = request_track.RequestTrack.FromJsonDict(
        json_dict[cls._REQUEST_KEY])
    tracing_track = tracing.TracingTrack.FromJsonDict(
        json_dict[cls._TRACING_KEY])
    return LoadingTrace(json_dict[cls._URL_KEY], json_dict[cls._METADATA_KEY],
                        page, request, tracing_track)

  @classmethod
  def FromJsonFile(cls, json_path):
    """Returns an instance from a json file saved by ToJsonFile()."""
    with open(json_path) as input_file:
      return cls.FromJsonDict(json.load(input_file))

  @classmethod
  def RecordUrlNavigation(
      cls, url, connection, chrome_metadata, categories=None,
      timeout_seconds=devtools_monitor.DEFAULT_TIMEOUT_SECONDS):
    """Create a loading trace by using controller to fetch url.

    Args:
      url: (str) url to fetch.
      connection: An opened devtools connection.
      chrome_metadata: Dictionary of chrome metadata.
      categories: TracingTrack categories to capture.
      timeout_seconds: monitoring connection timeout in seconds.

    Returns:
      LoadingTrace instance.
    """
    page = page_track.PageTrack(connection)
    request = request_track.RequestTrack(connection)
    trace = tracing.TracingTrack(
        connection,
        categories=(tracing.DEFAULT_CATEGORIES if categories is None
                    else categories))
    connection.MonitorUrl(url, timeout_seconds=timeout_seconds)
    return cls(url, chrome_metadata, page, request, trace)
