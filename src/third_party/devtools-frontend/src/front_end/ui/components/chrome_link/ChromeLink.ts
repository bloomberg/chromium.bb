// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as SDK from '../../../core/sdk/sdk.js';
import * as LitHtml from '../../lit-html/lit-html.js';
import * as ComponentHelpers from '../helpers/helpers.js';

import chromeLinkStyles from './chromeLink.css.js';

declare global {
  interface HTMLElementTagNameMap {
    'devtools-chrome-link': ChromeLink;
  }
}

// Use this component to render links to 'chrome://...'-URLs
// (for which regular <x-link>s do not work).
export class ChromeLink extends HTMLElement {
  static readonly litTagName = LitHtml.literal`devtools-chrome-link`;
  readonly #shadow = this.attachShadow({mode: 'open'});
  readonly #boundRender = this.#render.bind(this);
  #href: string = '';

  connectedCallback(): void {
    this.#shadow.adoptedStyleSheets = [chromeLinkStyles];
    void ComponentHelpers.ScheduledRender.scheduleRender(this, this.#boundRender);
  }

  set href(href: string) {
    if (!href.startsWith('chrome://')) {
      throw new Error('ChromeLink href needs to start with \'chrome://\'');
    }
    this.#href = href;
    void ComponentHelpers.ScheduledRender.scheduleRender(this, this.#boundRender);
  }

  // Navigating to a chrome:// link via a normal anchor doesn't work, so we "navigate"
  // there using CDP.
  openSettingsTab(event: Event): void {
    if (event.type === 'click' || (event.type === 'keydown' && self.isEnterOrSpaceKey(event))) {
      const mainTarget = SDK.TargetManager.TargetManager.instance().mainTarget();
      mainTarget && mainTarget.targetAgent().invoke_createTarget({url: this.#href});
      event.consume(true);
    }
  }

  #render(): void {
    // clang-format off
    LitHtml.render(
      /* x-link doesn't work with custom click/keydown handlers */
      /* eslint-disable rulesdir/ban_a_tags_in_lit_html */
      LitHtml.html`
        <a href=${this.#href} class="link" target="_blank"
          @click=${this.openSettingsTab}
          @keydown=${this.openSettingsTab}><slot></slot></a>
      `, this.#shadow, {host: this});
    // clang-format on
  }
}

ComponentHelpers.CustomElements.defineComponent('devtools-chrome-link', ChromeLink);
