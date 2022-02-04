// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import './strings.m.js';

import {ClickInfo, Command} from 'chrome://resources/js/browser_command/browser_command.mojom-webui.js';
import {BrowserCommandProxy} from 'chrome://resources/js/browser_command/browser_command_proxy.js';
import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WhatsNewProxyImpl} from './whats_new_proxy.js';

type CommandData = {
  commandId: number,
  clickInfo: ClickInfo,
};

// TODO (https://www.crbug.com/1219381): Add some additional parameters so
// that we can filter the messages a bit better.
type BrowserCommandMessageData = {
  data: CommandData,
};

export class WhatsNewAppElement extends PolymerElement {
  static get is() {
    return 'whats-new-app';
  }

  static get properties() {
    return {
      url_: {
        type: String,
        value: '',
      }
    };
  }

  private url_: string;

  private isAutoOpen_: boolean = false;
  private eventTracker_: EventTracker = new EventTracker();

  constructor() {
    super();

    const queryParams = new URLSearchParams(window.location.search);
    this.isAutoOpen_ = queryParams.has('auto');

    // There are no subpages in What's New. Also remove the query param here
    // since its value is recorded.
    window.history.replaceState(undefined /* stateObject */, '', '/');
  }

  connectedCallback() {
    super.connectedCallback();

    WhatsNewProxyImpl.getInstance().initialize().then(
        url => this.handleUrlResult_(url));
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  /**
   * Handles the URL result of sending the initialize WebUI message.
   * @param url The What's New URL to use in the iframe.
   */
  private handleUrlResult_(url: string|null) {
    if (!url) {
      // This occurs in the special case of tests where we don't want to load
      // remote content.
      return;
    }

    const latest = this.isAutoOpen_ && !isChromeOS ? 'true' : 'false';
    const feedback =
        loadTimeData.getBoolean('showFeedbackButton') ? 'true' : 'false';
    url += url.includes('?') ? '&' : '?';
    this.url_ = url.concat(`latest=${latest}&feedback=${feedback}`);

    this.eventTracker_.add(
        window, 'message', event => this.handleMessage_(event as MessageEvent));
  }

  private handleMessage_(event: MessageEvent) {
    if (!this.url_) {
      return;
    }

    const {data, origin} = event;
    const iframeUrl = new URL(this.url_);
    if (!data || origin !== iframeUrl.origin) {
      return;
    }

    const commandData = (data as BrowserCommandMessageData).data;
    if (!commandData) {
      return;
    }

    const commandId = Object.values(Command).includes(commandData.commandId) ?
        commandData.commandId :
        Command.kUnknownCommand;

    const handler = BrowserCommandProxy.getInstance().handler;
    handler.canExecuteCommand(commandId).then(({canExecute}) => {
      if (canExecute) {
        handler.executeCommand(commandId, commandData.clickInfo);
      } else {
        console.warn('Received invalid command: ' + commandId);
      }
    });
  }

  static get template() {
    return html`{__html_template__}`;
  }
}
customElements.define(WhatsNewAppElement.is, WhatsNewAppElement);
