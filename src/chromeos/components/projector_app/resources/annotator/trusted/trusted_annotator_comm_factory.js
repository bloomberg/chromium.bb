// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageAPIClient} from 'chrome://resources/js/post_message_api_client.m.js';
import {RequestHandler} from 'chrome://resources/js/post_message_api_request_handler.m.js';

import {ProjectorBrowserProxy, ProjectorBrowserProxyImpl} from '../../communication/projector_browser_proxy.js';

const TARGET_URL = 'chrome-untrusted://projector/';

// A PostMessageAPIClient that sends messages to chrome-untrusted://projector.
export class UntrustedAnnotatorClient extends PostMessageAPIClient {
  /**
   * @param {!Window} targetWindow
   */
  constructor(targetWindow) {
    super(TARGET_URL, targetWindow);
  }

  /**
   * Notifies the Annotator tool to update the tool.
   * @param {!projectorApp.AnnotatorToolParams} tool
   * @return {Promise<boolean>}
   */
  setTool(tool) {
    return this.callApiFn('setTool', [tool]);
  }

  /**
   * Notifies the Annotator to undo the last stroke.
   * @return {Promise<boolean>}
   */
  undo() {
    return this.callApiFn('undo', []);
  }

  /**
   * Notifies the Annotator to redo the last stroke.
   * @return {Promise<boolean>}
   */
  redo() {
    return this.callApiFn('redo', []);
  }

  /**
   * Notifies the Annotator to clear the screen.
   * @return {Promise<boolean>}
   */
  clear() {
    return this.callApiFn('clear', []);
  }
};

/**
 * Class that implements the RequestHandler inside the Projector trusted scheme
 * for Annotator.
 */
class TrustedAnnotatorRequestHandler extends RequestHandler {
  /*
   * @param {!Element} iframeElement The <iframe> element to listen to as a
   *     client.
   * @param {ProjectorBrowserProxy} browserProxy The browser proxy that will
   *     be used to handle the messages.
   */
  constructor(iframeElement, browserProxy) {
    super(iframeElement.contentWindow, TARGET_URL, TARGET_URL);
    this.browserProxy_ = browserProxy;

    this.registerMethod('onUndoRedoAvailabilityChanged', (values) => {
      if (!values || values.length != 2) {
        return;
      }
      return this.browserProxy_.onUndoRedoAvailabilityChanged(
          values[0], values[1]);
    });
  }
};

/**
 * This is a class that is used to setup the duplex communication
 * channels between this origin, chrome://projector/* and the iframe embedded
 * inside the document.
 */
export class AnnotatorTrustedCommFactory {
  /**
   * Creates the instances of PostMessageAPIClient and RequestHandler if they
   * have not been created already.
   */
  static maybeCreateInstances() {
    if (AnnotatorTrustedCommFactory.client_ ||
        AnnotatorTrustedCommFactory.requestHandler_) {
      return;
    }

    let iframeElement = document.getElementsByTagName('iframe')[0];

    AnnotatorTrustedCommFactory.client_ =
        new UntrustedAnnotatorClient(iframeElement.contentWindow);

    AnnotatorTrustedCommFactory.requestHandler_ =
        new TrustedAnnotatorRequestHandler(
            iframeElement, ProjectorBrowserProxyImpl.getInstance());
  }

  /**
   * In order to use this class, please do the following
   * (e.g. to set the tool do the following):
   * const success = await
   * AnnotatorTrustedCommFactory.getPostMessageAPIClient().setTool(tool);
   *
   * @return {!UntrustedAnnotatorClient}
   */
  static getPostMessageAPIClient() {
    // AnnotatorTrustedCommFactory.client_ should be available. However to be on
    // the cautious side create an instance here if getPostMessageAPIClient is
    // triggered before the page finishes loading.
    AnnotatorTrustedCommFactory.maybeCreateInstances();
    return AnnotatorTrustedCommFactory.client_;
  }
}

document.addEventListener('DOMContentLoaded', () => {
  // Create instances of the singletons(PostMessageAPIClient and
  // RequestHandler) when the document has finished loading.
  AnnotatorTrustedCommFactory.maybeCreateInstances();
});
