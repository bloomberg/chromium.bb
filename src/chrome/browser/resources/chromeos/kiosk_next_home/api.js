// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Chrome OS Kiosk Next Home API definition.
 */

/**
 * Namespace for the Kiosk Next Home bridge and related data.
 * @const
 */
var kioskNextHome = {};

/**
 * System bridge API for the Kiosk Next Home.
 *
 * @interface
 */
kioskNextHome.Bridge = class {
  /**
   * Adds listener for system events.
   * @param {!kioskNextHome.Listener} listener Listener for system events.
   */
  addListener(listener) {}

  /**
   * Returns an access token with the requested scopes.
   * @param {!Array<string>} scopes List of scopes to use when obtaining access
   *     token.
   * @return {!Promise<string>} Promise for the access token.
   */
  getAccessToken(scopes) {}

  /**
   * Returns a list of apps installed in the user session.
   * @return {!Promise<!Array<!kioskNextHome.InstalledApp>>} Promise for the
   *     list of apps.
   */
  getInstalledApps() {}

  /**
   * Launches a content (app, video, etc).
   * @param {!kioskNextHome.ContentSource} contentSource
   * @param {string} contentId
   * @param {?Object=} opt_params Optional params to locate the content.
   * @return {!Promise<boolean>} Promise that is resolved after the content is
   *     launched.
   */
  launchContent(contentSource, contentId, opt_params) {}
};

/**
 * Set of known / handled content sources.
 *
 * A "Content Source" describes how to launch/view the content.
 * @enum {string}
 */
kioskNextHome.ContentSource = {
  /** The content is, or is hosted inside, an ARC++ app. */
  ARC_INTENT: 'arc_intent',
};

/**
 * Types of installed apps on ChromeOS.
 * @enum {string}
 */
kioskNextHome.AppType = {
  /** The app is an ARC++ app (Android app). */
  ARC: 'arc',
};

/**
 * A record representing an installed app on the system.
 * @record
 */
kioskNextHome.InstalledApp = class {
  constructor() {
    /** @type {!kioskNextHome.AppType} The type of app. */
    this.appType;
    /**
     * @type {string} Stable, unique identifier for the app. For ARC++ apps,
     *     this is the package name.
     */
    this.appId;
    /** @type {string} Readable name to display. */
    this.displayName;
    /** @type {string | undefined} Base64-encoded thumbnail image, fallback. */
    this.thumbnailImage;
    /** @type {boolean | undefined} Whether the app is suspended. */
    this.suspended;
  }
};

/**
 * Different ways an installed app can change.
 * @enum {string}
 */
kioskNextHome.AppEventType = {
  INSTALLED: 'installed',
  UNINSTALLED: 'uninstalled',
};

/**
 * Interface for a listener of system events, subscribed via
 * {!kioskNextHome.Bridge}.
 *
 * @interface
 */
kioskNextHome.Listener = class {
  /**
   * Called when an app state change.
   * @param {!kioskNextHome.InstalledApp} app The app whose state changed.
   * @param {!kioskNextHome.AppEventType} appEventType Type of the event
   *     indicating what changed for the app.
   */
  onInstalledAppChanged(app, appEventType) {}
};

/**
 * Provides bridge implementation.
 * @return {!kioskNextHome.Bridge} Bridge instance that can be used to interact
 *     with ChromeOS.
 */
kioskNextHome.getChromeOsBridge = function() {};
