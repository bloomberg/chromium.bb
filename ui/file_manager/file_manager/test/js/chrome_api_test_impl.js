// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test implementation of chrome.* apis.
// These APIs are provided natively to a chrome app, but since we are
// running as a regular web page, we must provide test implementations.

chrome = {
  app: {
    runtime: {
      onLaunched: {
        addListener: () => {},
      },
      onRestarted: {
        addListener: () => {},
      },
    },
  },

  commandLinePrivate: {
    switches_: {},
    hasSwitch: (name, callback) => {
      console.debug('chrome.commandLinePrivate.hasSwitch called', name);
      setTimeout(callback, 0, chrome.commandLinePrivate.switches_[name]);
    },
  },

  contextMenus: {
    create: () => {},
    onClicked: {
      addListener: () => {},
    },
  },

  echoPrivate: {
    getOfferInfo: (id, callback) => {
      console.debug('chrome.echoPrivate.getOfferInfo called', id);
      setTimeout(() => {
        // checkSpaceAndMaybeShowWelcomeBanner_ relies on lastError being set.
        chrome.runtime.lastError = {message: 'Not found'};
        callback(undefined);
        chrome.runtime.lastError = undefined;
      }, 0);
    },
  },

  extension: {
    getViews: (fetchProperties) => {
      console.debug('chrome.extension.getViews called', fetchProperties);
      // Returns Window[].
      return [window];
    },
    inIncognitoContext: false,
  },

  fileBrowserHandler: {
    onExecute: {
      addListener: () => {},
    },
  },

  metricsPrivate: {
    MetricTypeType: {
      HISTOGRAM_LINEAR: 'histogram-linear',
    },
    recordMediumCount: () => {},
    recordSmallCount: () => {},
    recordTime: () => {},
    recordUserAction: () => {},
    recordValue: () => {},
  },

  notifications: {
    onButtonClicked: {
      addListener: () => {},
    },
    onClicked: {
      addListener: () => {},
    },
    onClosed: {
      addListener: () => {},
    },
  },

  runtime: {
    getBackgroundPage: (callback) => {
      setTimeout(callback, 0, window);
    },
    // FileManager extension ID.
    id: 'hhaomjibdihmijegdhdafkllkbggdgoj',
    onMessageExternal: {
      addListener: () => {},
    },
  },

  storage: {
    local: {
      get: (keys, callback) => {
        console.debug('chrome.storage.local.get', keys);
        setTimeout(callback, 0, {});
      },
      set: (items, callback) => {
        console.debug('chrome.storage.local.set', items);
        if (callback) {
          setTimeout(callback, 0);
        }
      },
    },
    onChanged: {
      addListener: () => {},
    },
    sync: {
      get: (keys, callback) => {
        console.debug('chrome.storage.sync.get', keys);
        setTimeout(callback, 0, {});
      }
    },
  },
};
