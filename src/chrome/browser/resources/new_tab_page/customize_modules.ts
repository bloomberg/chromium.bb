// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.m.js';

import {CrRadioButtonElement} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './customize_modules.html.js';
import {I18nMixin, loadTimeData} from './i18n_setup.js';
import {ChromeCartProxy} from './modules/cart/chrome_cart_proxy.js';
import {ModuleRegistry} from './modules/module_registry.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';

declare global {
  interface Window {
    CrPolicyStrings: {[key: string]: string},
  }
}

type ModuleSetting = {
  name: string,
  id: string,
  checked: boolean,
  initiallyChecked: boolean,
  disabled: boolean,
};


export interface CustomizeModulesElement {
  $: {
    customizeButton: CrRadioButtonElement,
    hideButton: CrRadioButtonElement,
    toggleRepeat: DomRepeat,
  };
}

/** Element that lets the user configure modules settings. */
export class CustomizeModulesElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-customize-modules';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * If true, modules are customizable. If false, all modules are hidden.
       */
      show_: {
        type: Boolean,
        observer: 'onShowChange_',
      },

      showManagedByPolicy_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesVisibleManagedByPolicy'),
      },

      modules_: {
        type: Array,
        value: () => ModuleRegistry.getInstance().getDescriptors().map(
            d => ({name: d.name, id: d.id, checked: true, hidden: false})),
      },

      // Discount toggle is a workaround for crbug.com/1199465 and will be
      // removed after module customization is better defined. Please avoid
      // using similar pattern for other features.
      discountToggle_: {
        type: Object,
        value: {enabled: false, initiallyEnabled: false},
      },

      discountToggleEligible_: {
        type: Boolean,
        value: false,
      }
    };
  }

  private show_: boolean;
  private showManagedByPolicy_: boolean;
  private modules_: ModuleSetting[];
  private discountToggle_: {enabled: boolean, initiallyEnabled: boolean};
  private discountToggleEligible_: boolean;

  private setDisabledModulesListenerId_: number|null = null;

  connectedCallback() {
    super.connectedCallback();
    this.setDisabledModulesListenerId_ =
        NewTabPageProxy.getInstance()
            .callbackRouter.setDisabledModules.addListener(
                (all: boolean, ids: string[]) => {
                  this.show_ = !all;
                  this.modules_.forEach(({id}, i) => {
                    const checked = !all && !ids.includes(id);
                    this.set(`modules_.${i}.checked`, checked);
                    this.set(`modules_.${i}.initiallyChecked`, checked);
                    this.set(`modules_.${i}.disabled`, ids.includes(id));
                  });
                });
    NewTabPageProxy.getInstance().handler.updateDisabledModules();
    this.set(
        'discountToggleEligible_',
        loadTimeData.getBoolean('ruleBasedDiscountEnabled'));
    if (!this.discountToggleEligible_) {
      return;
    }
    ChromeCartProxy.getHandler().getDiscountEnabled().then(({enabled}) => {
      this.set('discountToggle_.enabled', enabled);
      this.discountToggle_.initiallyEnabled = enabled;
    });
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    NewTabPageProxy.getInstance().callbackRouter.removeListener(
        this.setDisabledModulesListenerId_!);
  }

  ready() {
    // |window.CrPolicyStrings.controlledSettingPolicy| populates the tooltip
    // text of <cr-policy-indicator indicator-type="devicePolicy" /> elements.
    // Needs to be called before |super.ready()| so that the string is available
    // when <cr-policy-indicator> gets instantiated.
    window.CrPolicyStrings = {
      controlledSettingPolicy:
          loadTimeData.getString('controlledSettingPolicy'),
    };
    super.ready();
  }

  apply() {
    const handler = NewTabPageProxy.getInstance().handler;
    handler.setModulesVisible(this.show_);
    this.modules_
        .filter(({checked, initiallyChecked}) => checked !== initiallyChecked)
        .forEach(({id, checked}) => {
          // Don't set disabled status of a module if we are in hide all mode to
          // preserve the status for the next time we go into customize mode.
          if (this.show_) {
            handler.setModuleDisabled(id, !checked);
          }
          const base = `NewTabPage.Modules.${checked ? 'Enabled' : 'Disabled'}`;
          chrome.metricsPrivate.recordSparseHashable(base, id);
          chrome.metricsPrivate.recordSparseHashable(`${base}.Customize`, id);
        });
    // Discount toggle is a workaround for crbug.com/1199465 and will be
    // removed after module customization is better defined. Please avoid
    // using similar pattern for other features.
    if (this.discountToggleEligible_ &&
        this.discountToggle_.enabled !==
            this.discountToggle_.initiallyEnabled) {
      ChromeCartProxy.getHandler().setDiscountEnabled(
          this.discountToggle_.enabled);
      chrome.metricsPrivate.recordUserAction(`NewTabPage.Carts.${
          this.discountToggle_.enabled ? 'EnableDiscount' :
                                         'DisableDiscount'}`);
    }
  }

  private onShowRadioSelectionChanged_(e: CustomEvent<{value: string}>) {
    this.show_ = e.detail.value === 'customize';
  }

  private onShowChange_() {
    this.modules_.forEach(
        (m, i) => this.set(`modules_.${i}.checked`, this.show_ && !m.disabled));
  }

  private radioSelection_(): string {
    return this.show_ ? 'customize' : 'hide';
  }

  private moduleToggleDisabled_(): boolean {
    return this.showManagedByPolicy_ || !this.show_;
  }

  private showDiscountToggle_(
      id: string, checked: boolean, eligible: boolean): boolean {
    return id === 'chrome_cart' && checked && eligible;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-customize-modules': CustomizeModulesElement;
  }
}

customElements.define(CustomizeModulesElement.is, CustomizeModulesElement);
