// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines UMA analytics specific to the DIAL provider.
 */

goog.provide('mr.DialAnalytics');

goog.require('mr.Analytics');


/**
 * Contains all analytics logic for the DIAL provider.
 * @const {*}
 */
mr.DialAnalytics = {};


/** @enum {string} */
mr.DialAnalytics.Metric = {
  DEVICE_DESCRIPTION_FAILURE: 'MediaRouter.Dial.Device.Description.Failure',
  DEVICE_DESCRIPTION_FROM_CACHE: 'MediaRouter.Dial.Device.Description.Cached',
  DIAL_CREATE_ROUTE: 'MediaRouter.Dial.CreateRoute',
  DIAL_TERMINATE_ROUTE: 'MediaRouter.Dial.TerminateRoute',
  NON_CAST_DISCOVERY: 'MediaRouter.Dial.Sink.Discovered.NonCast',
  PARSE_MESSAGE: 'MediaRouter.Dial.ParseMessage'
};


/**
 * Possible values for the route creation analytics.
 * @enum {number}
 */
mr.DialAnalytics.DialCreateRouteResult = {
  SUCCESS: 0,
  SINK_NOT_FOUND: 1,
  APP_INFO_NOT_FOUND: 2,
  APP_LAUNCH_FAILED: 3,
  UNSUPPORTED_SOURCE: 4,
  ROUTE_ALREADY_EXISTS: 5
};


/** @enum {number} */
mr.DialAnalytics.DialTerminateRouteResult = {
  SUCCESS: 0,
  ROUTE_NOT_FOUND: 1,
  SINK_NOT_FOUND: 2,
  STOP_APP_FAILED: 3,
};


/**
 * Possible values for device description failures.
 * @enum {number}
 */
mr.DialAnalytics.DeviceDescriptionFailures = {
  ERROR: 0,
  PARSE: 1,
  EMPTY: 2
};


/** @enum {number} */
mr.DialAnalytics.DialParseMessageResult = {
  SUCCESS: 0,
  PARSE_ERROR: 1,
  INVALID_MESSAGE: 2
};


/**
 * Records analytics around route creation.
 * @param {!mr.DialAnalytics.DialCreateRouteResult} value
 */
mr.DialAnalytics.recordCreateRoute = function(value) {
  mr.Analytics.recordEnum(
      mr.DialAnalytics.Metric.DIAL_CREATE_ROUTE, value,
      mr.DialAnalytics.DialCreateRouteResult);
};


/** @param {!mr.DialAnalytics.DialTerminateRouteResult} value */
mr.DialAnalytics.recordTerminateRoute = function(value) {
  mr.Analytics.recordEnum(
      mr.DialAnalytics.Metric.DIAL_TERMINATE_ROUTE, value,
      mr.DialAnalytics.DialTerminateRouteResult);
};

/**
 * Records a failure with the device description.
 * @param {!mr.DialAnalytics.DeviceDescriptionFailures} value The failure
 * reason.
 */
mr.DialAnalytics.recordDeviceDescriptionFailure = function(value) {
  mr.Analytics.recordEnum(
      mr.DialAnalytics.Metric.DEVICE_DESCRIPTION_FAILURE, value,
      mr.DialAnalytics.DeviceDescriptionFailures);
};


/**
 * Records that device description was retreived from the cache.
 */
mr.DialAnalytics.recordDeviceDescriptionFromCache = function() {
  mr.Analytics.recordEvent(
      mr.DialAnalytics.Metric.DEVICE_DESCRIPTION_FROM_CACHE);
};


/**
 * Records that a device was discovered by DIAL that didn't support the cast
 * protocol.
 */
mr.DialAnalytics.recordNonCastDiscovery = function() {
  mr.Analytics.recordEvent(mr.DialAnalytics.Metric.NON_CAST_DISCOVERY);
};


/** @param {!mr.DialAnalytics.DialParseMessageResult} value */
mr.DialAnalytics.recordParseMessageResult = function(value) {
  mr.Analytics.recordEnum(
      mr.DialAnalytics.Metric.PARSE_MESSAGE, value,
      mr.DialAnalytics.DialParseMessageResult);
};


/**
 * Histogram name for available DIAL devices count.
 * @private @const {string}
 */
mr.DialAnalytics.AVAILABLE_DEVICES_COUNT_ =
    'MediaRouter.Dial.AvailableDevicesCount';


/**
 * Histogram name for known DIAL devices count.
 * @private @const {string}
 */
mr.DialAnalytics.KNOWN_DEVICES_COUNT_ = 'MediaRouter.Dial.KnownDevicesCount';


/**
 * Records device counts.
 * @param {!mr.DeviceCounts} deviceCounts
 */
mr.DialAnalytics.recordDeviceCounts = function(deviceCounts) {
  mr.Analytics.recordSmallCount(
      mr.DialAnalytics.AVAILABLE_DEVICES_COUNT_,
      deviceCounts.availableDeviceCount);
  mr.Analytics.recordSmallCount(
      mr.DialAnalytics.KNOWN_DEVICES_COUNT_, deviceCounts.knownDeviceCount);
};
