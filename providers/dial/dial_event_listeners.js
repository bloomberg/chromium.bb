// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Event listeners for DIAL MRP.
 */

goog.provide('mr.dial.EventListeners');

goog.require('mr.EventAnalytics');
goog.require('mr.EventListener');


/**
 * Event listener for chrome.dial.onDeviceList. Added when
 * mr.dial.SinkDiscoveryService is ready to discover DIAL devices.
 * @private {?mr.EventListener<ChromeEvent>}
 */
mr.dial.EventListeners.dialOnDeviceListEventListener_ = null;


/**
 * Event listener for chrome.dial.onError. Added when
 * mr.dial.SinkDiscoveryService is ready to discover DIAL devices.
 * @private {?mr.EventListener<ChromeEvent>}
 */
mr.dial.EventListeners.dialOnErrorEventListener_ = null;


/**
 * @return {!Array<!mr.EventListener<ChromeEvent>>}
 */
mr.dial.EventListeners.getAllListeners = function() {
  if (!mr.dial.EventListeners.dialOnDeviceListEventListener_) {
    mr.dial.EventListeners.dialOnDeviceListEventListener_ =
        new mr.EventListener(
            mr.EventAnalytics.Event.DIAL_ON_DEVICE_LIST,
            'dial.DialOnDeviceListEventListener',
            mr.ModuleId.DIAL_SINK_DISCOVERY_SERVICE, chrome.dial.onDeviceList);
  }
  if (!mr.dial.EventListeners.dialOnErrorEventListener_) {
    mr.dial.EventListeners.dialOnErrorEventListener_ = new mr.EventListener(
        mr.EventAnalytics.Event.DIAL_ON_ERROR, 'dial.DialOnErrorEventListener',
        mr.ModuleId.DIAL_SINK_DISCOVERY_SERVICE, chrome.dial.onError);
  }
  return [
    mr.dial.EventListeners.dialOnDeviceListEventListener_,
    mr.dial.EventListeners.dialOnErrorEventListener_
  ];
};
