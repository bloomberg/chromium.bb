// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './history_clusters_shared_style.css.js';
import '../../cr_elements/cr_action_menu/cr_action_menu.js';
import '../../cr_elements/cr_icon_button/cr_icon_button.m.js';
import '../../cr_elements/cr_lazy_render/cr_lazy_render.m.js';

import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrActionMenuElement} from '../../cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from '../../cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {loadTimeData} from '../../js/load_time_data.m.js';

import {URLVisit} from './history_clusters.mojom-webui.js';
import {getTemplate} from './menu_container.html.js';

/**
 * @fileoverview This file provides a custom element displaying an action menu.
 * It's meant to be flexible enough to be associated with either a specific
 * visit, or the whole cluster, or the top visit of unlabelled cluster.
 */

declare global {
  interface HTMLElementTagNameMap {
    'menu-container': MenuContainerElement;
  }
}

const MenuContainerElementBase = I18nMixin(PolymerElement);

interface MenuContainerElement {
  $: {
    actionMenu: CrLazyRenderElement<CrActionMenuElement>,
    actionMenuButton: HTMLElement,
  };
}

class MenuContainerElement extends MenuContainerElementBase {
  static get is() {
    return 'menu-container';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The visit associated with this menu.
       */
      visit: Object,

      /**
       * Usually this is true, but this can be false if deleting history is
       * prohibited by Enterprise policy.
       */
      allowDeletingHistory_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('allowDeletingHistory'),
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  visit: URLVisit;
  private allowDeletingHistory_: boolean;

  //============================================================================
  // Event handlers
  //============================================================================

  private onActionMenuButtonClick_(event: Event) {
    this.$.actionMenu.get().showAt(this.$.actionMenuButton);
    event.preventDefault();  // Prevent default browser action (navigation).
  }

  private onOpenAllButtonClick_(event: Event) {
    event.preventDefault();  // Prevent default browser action (navigation).

    this.dispatchEvent(new CustomEvent('open-all-visits', {
      bubbles: true,
      composed: true,
    }));

    this.$.actionMenu.get().close();
  }

  private onRemoveAllButtonClick_(event: Event) {
    event.preventDefault();  // Prevent default browser action (navigation).

    this.dispatchEvent(new CustomEvent('remove-all-visits', {
      bubbles: true,
      composed: true,
    }));

    this.$.actionMenu.get().close();
  }
}

customElements.define(MenuContainerElement.is, MenuContainerElement);
