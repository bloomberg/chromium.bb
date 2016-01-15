# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from request_track import (Request, RequestTrack, _TimingFromDict)


class RequestTrackTestCase(unittest.TestCase):
  _REQUEST_WILL_BE_SENT = {
      'method': 'Network.requestWillBeSent',
      'params': {
          'documentURL': 'http://example.com/',
          'frameId': '32493.1',
          'initiator': {
              'type': 'other'
              },
          'loaderId': '32493.3',
          'request': {
              'headers': {
                  'Accept': 'text/html',
                  'Upgrade-Insecure-Requests': '1',
                  'User-Agent': 'Mozilla/5.0'
                  },
              'initialPriority': 'VeryHigh',
              'method': 'GET',
              'mixedContentType': 'none',
              'url': 'http://example.com/'
              },
          'requestId': '32493.1',
          'timestamp': 5571441.535053,
          'type': 'Document',
          'wallTime': 1452691674.08878}}
  _REDIRECT = {
      'method': 'Network.requestWillBeSent',
      'params': {
          'documentURL': 'http://www.example.com/',
          'frameId': '32493.1',
          'initiator': {
              'type': 'other'
              },
          'loaderId': '32493.3',
          'redirectResponse': {
              'connectionId': 18,
              'connectionReused': False,
              'encodedDataLength': 198,
              'fromDiskCache': False,
              'fromServiceWorker': False,
              'headers': {},
              'headersText': 'HTTP/1.1 301 Moved Permanently\r\n',
              'mimeType': 'text/html',
              'protocol': 'http/1.1',
              'remoteIPAddress': '216.146.46.10',
              'remotePort': 80,
              'requestHeaders': {
                  'Accept': 'text/html',
                  'User-Agent': 'Mozilla/5.0'
                  },
              'securityState': 'neutral',
              'status': 301,
              'statusText': 'Moved Permanently',
              'timing': {
                  'connectEnd': 137.435999698937,
                  'connectStart': 51.1459996923804,
                  'dnsEnd': 51.1459996923804,
                  'dnsStart': 0,
                  'proxyEnd': -1,
                  'proxyStart': -1,
                  'receiveHeadersEnd': 228.187000378966,
                  'requestTime': 5571441.55002,
                  'sendEnd': 138.841999694705,
                  'sendStart': 138.031999580562,
                  'sslEnd': -1,
                  'sslStart': -1,
                  'workerReady': -1,
                  'workerStart': -1
                  },
              'url': 'http://example.com/'
              },
          'request': {
              'headers': {
                  'Accept': 'text/html',
                  'User-Agent': 'Mozilla/5.0'
                  },
              'initialPriority': 'VeryLow',
              'method': 'GET',
              'mixedContentType': 'none',
              'url': 'http://www.example.com/'
              },
          'requestId': '32493.1',
          'timestamp': 5571441.795948,
          'type': 'Document',
          'wallTime': 1452691674.34968}}
  _RESPONSE_RECEIVED = {
      'method': 'Network.responseReceived',
      'params': {
          'frameId': '32493.1',
          'loaderId': '32493.3',
          'requestId': '32493.1',
          'response': {
              'connectionId': 26,
              'connectionReused': False,
              'encodedDataLength': -1,
              'fromDiskCache': False,
              'fromServiceWorker': False,
              'headers': {
                  'Age': '67',
                  'Cache-Control': 'max-age=0,must-revalidate',
                  },
              'headersText': 'HTTP/1.1 200 OK\r\n',
              'mimeType': 'text/html',
              'protocol': 'http/1.1',
              'requestHeaders': {
                  'Accept': 'text/html',
                    'Host': 'www.example.com',
                    'User-Agent': 'Mozilla/5.0'
                },
                'status': 200,
                'timing': {
                    'connectEnd': 37.9800004884601,
                    'connectStart': 26.8250005319715,
                    'dnsEnd': 26.8250005319715,
                    'dnsStart': 0,
                    'proxyEnd': -1,
                    'proxyStart': -1,
                    'receiveHeadersEnd': 54.9750002101064,
                    'requestTime': 5571441.798671,
                    'sendEnd': 38.3980004116893,
                    'sendStart': 38.1810003891587,
                    'sslEnd': -1,
                    'sslStart': -1,
                    'workerReady': -1,
                    'workerStart': -1
                },
                'url': 'http://www.example.com/'
            },
            'timestamp': 5571441.865639,
            'type': 'Document'}}
  _DATA_RECEIVED_1 = {
      "method": "Network.dataReceived",
      "params": {
          "dataLength": 1803,
          "encodedDataLength": 1326,
          "requestId": "32493.1",
          "timestamp": 5571441.867347}}
  _DATA_RECEIVED_2 = {
      "method": "Network.dataReceived",
      "params": {
          "dataLength": 32768,
          "encodedDataLength": 32768,
          "requestId": "32493.1",
          "timestamp": 5571441.893121}}
  _LOADING_FINISHED = {'method': 'Network.loadingFinished',
                       'params': {
                           'encodedDataLength': 101829,
                           'requestId': '32493.1',
                           'timestamp': 5571441.891189}}

  def setUp(self):
    self.request_track = RequestTrack(None)

  def testParseRequestWillBeSent(self):
    msg = RequestTrackTestCase._REQUEST_WILL_BE_SENT
    request_id = msg['params']['requestId']
    self.request_track.Handle('Network.requestWillBeSent', msg)
    self.assertTrue(request_id in self.request_track._requests_in_flight)
    (_, status) = self.request_track._requests_in_flight[request_id]
    self.assertEquals(RequestTrack._STATUS_SENT, status)

  def testRejectsUnknownMethod(self):
    with self.assertRaises(AssertionError):
      self.request_track.Handle(
          'unknown', RequestTrackTestCase._REQUEST_WILL_BE_SENT)

  def testHandleRedirect(self):
    self.request_track.Handle('Network.requestWillBeSent',
                              RequestTrackTestCase._REQUEST_WILL_BE_SENT)
    self.request_track.Handle('Network.requestWillBeSent',
                              RequestTrackTestCase._REDIRECT)
    self.assertEquals(1, len(self.request_track._requests_in_flight))
    self.assertEquals(1, len(self.request_track.GetEvents()))

  def testRejectDuplicates(self):
    msg = RequestTrackTestCase._REQUEST_WILL_BE_SENT
    self.request_track.Handle('Network.requestWillBeSent', msg)
    with self.assertRaises(AssertionError):
      self.request_track.Handle('Network.requestWillBeSent', msg)

  def testInvalidSequence(self):
    msg1 = RequestTrackTestCase._REQUEST_WILL_BE_SENT
    msg2 = RequestTrackTestCase._LOADING_FINISHED
    self.request_track.Handle('Network.requestWillBeSent', msg1)
    with self.assertRaises(AssertionError):
      self.request_track.Handle('Network.loadingFinished', msg2)

  def testValidSequence(self):
    self._ValidSequence(self.request_track)
    self.assertEquals(1, len(self.request_track.GetEvents()))
    self.assertEquals(0, len(self.request_track._requests_in_flight))
    r = self.request_track.GetEvents()[0]
    self.assertEquals('32493.1', r.request_id)
    self.assertEquals('32493.1', r.frame_id)
    self.assertEquals('32493.3', r.loader_id)
    self.assertEquals('http://example.com/', r.document_url)
    self.assertEquals('http://example.com/', r.url)
    self.assertEquals('http/1.1', r.protocol)
    self.assertEquals('GET', r.method)
    response = RequestTrackTestCase._RESPONSE_RECEIVED['params']['response']
    self.assertEquals(response['requestHeaders'], r.request_headers)
    self.assertEquals(response['headers'], r.response_headers)
    self.assertEquals('VeryHigh', r.initial_priority)
    request_will_be_sent = (
        RequestTrackTestCase._REQUEST_WILL_BE_SENT['params'])
    self.assertEquals(request_will_be_sent['timestamp'], r.timestamp)
    self.assertEquals(request_will_be_sent['wallTime'], r.wall_time)
    self.assertEquals(request_will_be_sent['initiator'], r.initiator)
    self.assertEquals(request_will_be_sent['type'], r.resource_type)
    self.assertEquals(False, r.served_from_cache)
    self.assertEquals(False, r.from_disk_cache)
    self.assertEquals(False, r.from_service_worker)
    timing = _TimingFromDict(response['timing'])
    loading_finished = RequestTrackTestCase._LOADING_FINISHED['params']
    loading_finished_offset = r._TimestampOffsetFromStartMs(
        loading_finished['timestamp'])
    timing = timing._replace(loading_finished=loading_finished_offset)
    self.assertEquals(timing, r.timing)
    self.assertEquals(200, r.status)
    self.assertEquals(
        loading_finished['encodedDataLength'], r.encoded_data_length)
    self.assertEquals(False, r.failed)

  def testDataReceived(self):
    self._ValidSequence(self.request_track)
    self.assertEquals(1, len(self.request_track.GetEvents()))
    r = self.request_track.GetEvents()[0]
    self.assertEquals(2, len(r.data_chunks))
    self.assertEquals(
        RequestTrackTestCase._DATA_RECEIVED_1['params']['encodedDataLength'],
        r.data_chunks[0][1])
    self.assertEquals(
        RequestTrackTestCase._DATA_RECEIVED_2['params']['encodedDataLength'],
        r.data_chunks[1][1])

  @classmethod
  def _ValidSequence(cls, request_track):
    request_track.Handle(
        'Network.requestWillBeSent', cls._REQUEST_WILL_BE_SENT)
    request_track.Handle('Network.responseReceived', cls._RESPONSE_RECEIVED)
    request_track.Handle('Network.dataReceived', cls._DATA_RECEIVED_1)
    request_track.Handle('Network.dataReceived', cls._DATA_RECEIVED_2)
    request_track.Handle('Network.loadingFinished', cls._LOADING_FINISHED)

if __name__ == '__main__':
  unittest.main()
