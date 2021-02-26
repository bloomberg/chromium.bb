// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './http_message_object.js';
import './shared_style.js';

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NearbyHttpBrowserProxy} from './nearby_http_browser_proxy.js';
import {HttpMessage} from './types.js';

Polymer({
  is: 'http-tab',

  _template: html`{__html_template__}`,

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {

    /**
     * @private {!Array<!HttpMessage>}
     */
    httpMessageList_: {
      type: Array,
      value: [],
    },
  },

  /** @private {?NearbyHttpBrowserProxy} */
  browserProxy_: null,

  /**
   * Set |browserProxy_|.
   * @override
   */
  created() {
    this.browserProxy_ = NearbyHttpBrowserProxy.getInstance();
  },

  /**
   * When the page is initialized, notify the C++ layer to allow JavaScript and
   * initialize WebUI Listeners.
   * @override
   */
  attached() {
    this.addWebUIListener(
        'http-message-added', message => this.onHttpMessageAdded_(message));
    this.browserProxy_.initialize();
  },

  /**
   * Triggers UpdateDevice RPC.
   * @private
   */
  onUpdateDeviceClicked_() {
    this.browserProxy_.updateDevice();
  },

  /**
   * Triggers ListContactPeople RPC.
   * @private
   */
  onListContactPeopleClicked_() {
    this.browserProxy_.listContactPeople();
  },

  /**
   * Triggers ListPublicCertificates RPC.
   * @private
   */
  onListPublicCertificatesClicked_() {
    this.browserProxy_.listPublicCertificates();
  },

  /**
   * Clears the |httpMessageList_| messages displayed on the page.
   * @private
   */
  onClearMessagesButtonClicked_() {
    this.httpMessageList_ = [];
  },

  /**
   * Adds a HTTP message to the javascript message list displayed. Called from
   * the C++ WebUI handler when a HTTP message is created in response to a Rpc.
   * @param {!HttpMessage} message
   * @private
   */
  onHttpMessageAdded_(message) {
    this.unshift('httpMessageList_', message);
  },
});
