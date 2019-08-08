// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview A helper object used by the "Set Time" dialog. */
cr.define('settime', function() {
  /** @interface */
  class SetTimeBrowserProxy {
    /** Notifies C++ code that it's safe to call JS functions. */
    sendPageReady() {}

    /** @param {number} timeInSeconds */
    setTimeInSeconds(timeInSeconds) {}

    /** @param {string} timezone */
    setTimezone(timezone) {}

    /** Closes the dialog. */
    dialogClose() {}
  }

  /** @implements {settime.SetTimeBrowserProxy} */
  class SetTimeBrowserProxyImpl {
    /** @override */
    sendPageReady() {
      chrome.send('setTimePageReady');
    }

    /** @override */
    setTimeInSeconds(timeInSeconds) {
      chrome.send('setTimeInSeconds', [timeInSeconds]);
    }

    /** @override */
    setTimezone(timezone) {
      chrome.send('setTimezone', [timezone]);
    }

    /** @override */
    dialogClose() {
      chrome.send('dialogClose');
    }
  }

  cr.addSingletonGetter(SetTimeBrowserProxyImpl);

  return {
    SetTimeBrowserProxy: SetTimeBrowserProxy,
    SetTimeBrowserProxyImpl: SetTimeBrowserProxyImpl,
  };
});
