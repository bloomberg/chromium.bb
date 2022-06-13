// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview settings-startup-url-entry represents a UI component that
 * displays a URL that is loaded during startup. It includes a menu that allows
 * the user to edit/remove the entry.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import '../settings_shared_css.js';
import '../site_favicon.js';

import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {StartupPageInfo, StartupUrlsPageBrowserProxyImpl} from './startup_urls_page_browser_proxy.js';

/**
 * The name of the event fired from this element when the "Edit" option is
 * clicked.
 */
export const EDIT_STARTUP_URL_EVENT: string = 'edit-startup-url';

const SettingsStartupUrlEntryElementBase =
    mixinBehaviors([FocusRowBehavior], PolymerElement) as
    {new (): PolymerElement & FocusRowBehavior};

export class SettingsStartupUrlEntryElement extends
    SettingsStartupUrlEntryElementBase {
  static get is() {
    return 'settings-startup-url-entry';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      editable: {
        type: Boolean,
        reflectToAttribute: true,
      },

      model: Object,
    };
  }

  editable: boolean;
  model: StartupPageInfo;

  private onRemoveTap_() {
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
    StartupUrlsPageBrowserProxyImpl.getInstance().removeStartupPage(
        this.model.modelIndex);
  }

  private onEditTap_(e: Event) {
    e.preventDefault();
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
    this.dispatchEvent(new CustomEvent(EDIT_STARTUP_URL_EVENT, {
      bubbles: true,
      composed: true,
      detail: {
        model: this.model,
        anchor: this.shadowRoot!.querySelector('#dots'),
      },
    }));
  }

  private onDotsTap_() {
    const actionMenu = (this.shadowRoot!.querySelector('#menu') as
                        CrLazyRenderElement<CrActionMenuElement>)
                           .get();
    actionMenu.showAt(assert(this.shadowRoot!.querySelector('#dots')!));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-startup-url-entry': SettingsStartupUrlEntryElement;
  }
}

customElements.define(
    SettingsStartupUrlEntryElement.is, SettingsStartupUrlEntryElement);
