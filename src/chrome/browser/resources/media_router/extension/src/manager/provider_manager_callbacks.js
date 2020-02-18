// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview API used by Media Route Providers to interact with the Media
 * Route Provider Manager.
 */

goog.provide('mr.ProviderManagerCallbacks');
goog.provide('mr.ProviderManagerIssueCallbacks');
goog.provide('mr.ProviderManagerMirrorServiceCallbacks');
goog.provide('mr.ProviderManagerRouteCallbacks');
goog.provide('mr.ProviderManagerSinkCallbacks');

goog.require('mr.EventTarget');
goog.require('mr.mirror.Activity');


/**
 * @record
 */
mr.ProviderManagerSinkCallbacks = class {
  /**
   * Called to notify that sinks have been added, removed, or updated.
   */
  onSinksUpdated() {}

  /**
   * Called to notify that sink availability has changed.
   *

   *
   * @param {!mr.Provider} provider
   * @param {!mr.SinkAvailability} availability
   */
  onSinkAvailabilityUpdated(provider, availability) {}
};



/**
 * @record
 */
mr.ProviderManagerRouteCallbacks = class {
  /**
   * @param {!mr.Provider} provider
   * @param {!mr.Route} route
   */
  onRouteAdded(provider, route) {}

  /**
   * @param {!mr.Provider} provider
   * @param {!mr.Route} route
   */
  onRouteRemoved(provider, route) {}

  /**
   * @param {!mr.Provider} provider
   * @param {!mr.Route} route
   */
  onRouteUpdated(provider, route) {}

  /**
   * Sends the provided message to the web app. If the message should also be
   * sent internally via the MessagePort, also call onInternalMessage.
   * @param {!mr.Provider} provider
   * @param {string} routeId
   * @param {string|!Uint8Array|!Object} message If not a string (text message)
   *     or Uint8Array (binary message), then the message will be serialized to
   *     a JSON string and sent as a text message.
   */
  onRouteMessage(provider, routeId, message) {}

  /**
   * Called to notify the presentation connected to a route has changed state.
   * If the state is CLOSED, onPresentationConnectionClosed is called instead of
   * this method.
   * @param {!string} routeId
   * @param {!mr.PresentationConnectionState} state
   */
  onPresentationConnectionStateChanged(routeId, state) {}

  /**
   * Called to notify the presentation connected to a route has closed.
   * @param {string} routeId
   * @param {!mr.PresentationConnectionCloseReason} reason
   * @param {string} message
   */
  onPresentationConnectionClosed(routeId, reason, message) {}
};



/**
 * @record
 */
mr.ProviderManagerIssueCallbacks = class {
  /**
   * Sends the given issue to MediaRouter.
   * @param {!mr.Issue} issue
   */
  sendIssue(issue) {}
};



/**
 * @record
 * @extends {mr.ProviderManagerIssueCallbacks}
 */
mr.ProviderManagerMirrorServiceCallbacks = class {
  /**
   * Invoked by mirror service when it stops a mirror session due to error,
   * the end of a stream etc. Because provider manager is unaware of mirror
   * session's internal state, this method is used by mirror service to tell
   * provider manager to close the corresponding route.
   * @param {string} routeId
   */
  onMirrorSessionEnded(routeId) {}

  /**
   * Invoked by the mirror service when the mirroring activity description for
   * mirroring route |route| has changed.
   *
   * @param {!mr.Route} route
   * @param {!mr.mirror.Activity} mirrorActivity
   */
  handleMirrorActivityUpdate(route, mirrorActivity) {}

  /**
   * Invoked by the mirror service to start Tab mirroring use the Mirroring
   * Service.
   * @param {number} targetTabId
   * @param {!mojo.InterfaceRequest} request
   */
  getMirroringServiceHostForTab(targetTabId, request) {}

  /**
   * Invoked by the mirror service to start Desktop mirroring use the Mirroring
   * Service.
   * @param {number} initiatorTabId
   * @param {string} desktopStreamId
   * @param {!mojo.InterfaceRequest} request
   */
  getMirroringServiceHostForDesktop(initiatorTabId, desktopStreamId, request) {}


  /**
   * Invoked by the mirror service to start OffscreenTab mirroring use the
   * Mirroring Service.
   * @param {!mojo.Url} presentationUrl
   * @param {string} presentationId
   * @param {!mojo.InterfaceRequest} request
   */
  getMirroringServiceHostForOffscreenTab(
      presentationUrl, presentationId, request) {}
};



/**
 * @record
 * @extends {mr.ProviderManagerIssueCallbacks}
 * @extends {mr.ProviderManagerSinkCallbacks}
 * @extends {mr.ProviderManagerRouteCallbacks}
 * @extends {mr.ProviderManagerMirrorServiceCallbacks}
 */
mr.ProviderManagerCallbacks = class {
  /**
   * @param {string} routeId
   * @return {mr.Provider}
   */
  getProviderFromRouteId(routeId) {}

  /**
   * @return {!mr.EventTarget}
   */
  getRouteMessageEventTarget() {}

  /**
   * Sends the message internally, to the mirror session associated with the
   * provided route ID, via the MessagePort.
   * @param {!mr.Provider} provider
   * @param {!string} routeId
   * @param {!Object|string} message
   */
  onInternalMessage(provider, routeId, message) {}

  /**
   * Called by a provider to request the mirroring service to start mirroring.
   * @param {!mr.Provider} provider
   * @param {!mr.Route} route
   * @param {string=} opt_presentationId
   * @param {(function(!mr.Route): !mr.CancellablePromise<!mr.Route>)=}
   *     opt_streamStartedCallback Callback to invoke after stream capture
   *         succeeded and before the mirror session is created. This allows the
   *         provider to perform additional setup and update the route for the
   *         mirror session.
   * @return {!mr.CancellablePromise<!mr.Route>}
   */
  startMirroring(
      provider, route, opt_presentationId, opt_streamStartedCallback) {}

  /**
   * Called by a provider to request the mirroring service to update |route| to
   * the new source |sourceUrn|.
   * @param {!mr.Provider} provider
   * @param {!mr.Route} route
   * @param {string} sourceUrn
   * @param {string=} opt_presentationId
   * @param {number=} opt_tabId
   * @param {(function(!mr.Route): !mr.CancellablePromise)=}
   *     opt_streamStartedCallback Callback to invoke after stream capture
   *         succeeded and before the mirror session is created. This allows the
   *         provider to perform additional setup and update the route for the
   *         mirror session.
   * @return {!mr.CancellablePromise<!mr.Route>}
   */
  updateMirroring(
      provider, route, sourceUrn, opt_presentationId, opt_tabId,
      opt_streamStartedCallback) {}

  /**
   * Called by a component with |keepAlive| set to true when it requires the
   * extension to be kept alive. When a component no longer requires the
   * extension
   * to be kept alive, this method should be called with |keepAlive| set to
   * false.
   * The extension will be prevented from suspending as long as at least one
   * component requested keep alive.
   * @param {string} componentId Globally unique id of the requesting component.
   * @param {boolean} keepAlive
   */
  requestKeepAlive(componentId, keepAlive) {}
};
