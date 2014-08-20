# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# FIXME: Master name is deprecated. Remove it once all the bots have stopped
# uploading with that name.
#
# FIXME: The concept of groups is being deprecated.  Once it's removed from
# the flakiness dashboard, this whole map can go away.
_masters = {
    'chromium.chromiumos': {
        'name': 'ChromiumChromiumOS',
        'groups': ['@ToT ChromeOS']
    },
    'chromium.fyi': {
        'name': 'ChromiumFYI',
        'groups': ['@ToT Chromium FYI']
    },
    'chromium.gpu': {
        'name': 'ChromiumGPU',
        'groups': ['@ToT Chromium']
    },
    'chromium.gpu.fyi': {
        'name': 'ChromiumGPUFYI',
        'groups': ['@ToT Chromium FYI']
    },
    'chromium.linux': {
        'name': 'ChromiumLinux',
        'groups': ['@ToT Chromium']
    },
    'chromium.mac': {
        'name': 'ChromiumMac',
        'groups': ['@ToT Chromium']
    },
    'chromium.webkit': {
        'name': 'ChromiumWebkit',
        'groups': ['@ToT Chromium', '@ToT Blink'],
    },
    'chromium.win': {
        'name': 'ChromiumWin',
        'groups': ['@ToT Chromium']
    },
    'tryserver.chromium.gpu': {
        'name': 'GpuTryServer',
        'groups': ['TryServers']
    },
    'client.v8': {
        'name': 'V8',
        'groups': ['@ToT V8']
    },
}

_master_name_to_url_name = dict((v['name'], k) for k, v in _masters.items())


def getMaster(url_name):
  result = _masters.get(url_name)
  if result:
    # Note: we copy off result['groups'] to ensure a full deep copy of the
    # data.
    return {
      'name': result['name'],
      'url_name': url_name,
      'groups': result['groups'][:]
    }
  return None


def getMasterByMasterName(master_name):
  url_name = _master_name_to_url_name.get(master_name)
  if url_name:
    return getMaster(url_name)
  return None


def getAllMasters():
  for master in _masters:
    yield getMaster(master)
