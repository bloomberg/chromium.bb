// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as i18n from '../core/i18n/i18n.js';
import * as LitHtml from '../third_party/lit-html/lit-html.js';
import * as Components from '../ui/components/components.js';

const UIStrings = {
  /**
  *@description Tooltip text that appears when hovering over a valid address in the address line in the Linear Memory Inspector
  */
  enterAddress: 'Enter address',
  /**
  *@description Tooltip text that appears when hovering over the button to go back in history in the Linear Memory Navigator
  */
  goBackInAddressHistory: 'Go back in address history',
  /**
  *@description Tooltip text that appears when hovering over the button to go forward in history in the Linear Memory Navigator
  */
  goForwardInAddressHistory: 'Go forward in address history',
  /**
  *@description Tooltip text that appears when hovering over the page back icon in the Linear Memory Navigator
  */
  previousPage: 'Previous page',
  /**
  *@description Tooltip text that appears when hovering over the next page icon in the Linear Memory Navigator
  */
  nextPage: 'Next page',
  /**
  *@description Text to refresh the page
  */
  refresh: 'Refresh',
};
const str_ = i18n.i18n.registerUIStrings('linear_memory_inspector/LinearMemoryNavigator.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
const {render, html} = LitHtml;


export const enum Navigation {
  Backward = 'Backward',
  Forward = 'Forward',
}

export class AddressInputChangedEvent extends Event {
  data: {address: string, mode: Mode};

  constructor(address: string, mode: Mode) {
    super('address-input-changed');
    this.data = {address, mode};
  }
}

export class PageNavigationEvent extends Event {
  data: Navigation;

  constructor(navigation: Navigation) {
    super('page-navigation', {});
    this.data = navigation;
  }
}

export class HistoryNavigationEvent extends Event {
  data: Navigation;

  constructor(navigation: Navigation) {
    super('history-navigation', {});
    this.data = navigation;
  }
}

export class RefreshRequestedEvent extends Event {
  constructor() {
    super('refresh-requested', {});
  }
}

export interface LinearMemoryNavigatorData {
  address: string;
  mode: Mode;
  canGoBackInHistory: boolean;
  canGoForwardInHistory: boolean;
  valid: boolean;
  error: string|undefined;
}

export const enum Mode {
  Edit = 'Edit',
  Submitted = 'Submitted',
  InvalidSubmit = 'InvalidSubmit',
}

export class LinearMemoryNavigator extends HTMLElement {
  private readonly shadow = this.attachShadow({mode: 'open'});
  private address = '0';
  private error: string|undefined = undefined;
  private valid = true;
  private canGoBackInHistory = false;
  private canGoForwardInHistory = false;

  set data(data: LinearMemoryNavigatorData) {
    this.address = data.address;
    this.error = data.error;
    this.valid = data.valid;
    this.canGoBackInHistory = data.canGoBackInHistory;
    this.canGoForwardInHistory = data.canGoForwardInHistory;
    this.render();

    const addressInput = this.shadow.querySelector<HTMLInputElement>('.address-input');
    if (addressInput) {
      if (data.mode === Mode.Submitted) {
        addressInput.blur();
      } else if (data.mode === Mode.InvalidSubmit) {
        addressInput.select();
      }
    }
  }

  private render(): void {
    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    const result = html`
      <style>
        .navigator {
          min-height: 24px;
          display: flex;
          flex-wrap: nowrap;
          justify-content: space-between;
          overflow: hidden;
          align-items: center;
          background-color: var(--color-background);
          color: var(--color-text-primary);
        }

        .navigator-item {
          display: flex;
          white-space: nowrap;
          overflow: hidden;
        }

        .address-input {
          text-align: center;
          outline: none;
          color: var(--color-text-primary);
          border: 1px solid var(--color-background-elevation-2);
          background: transparent;
        }

        .address-input.invalid {
          color: var(--color-accent-red);
        }

        .navigator-button {
          display: flex;
          width: 20px;
          height: 20px;
          background: transparent;
          overflow: hidden;
          border: none;
          padding: 0;
          outline: none;
          justify-content: center;
          align-items: center;
          cursor: pointer;
        }

        .navigator-button devtools-icon {
          height: 14px;
          width: 14px;
          min-height: 14px;
          min-width: 14px;
        }

        .navigator-button:enabled:hover devtools-icon {
          --icon-color: var(--color-text-primary);
        }

        .navigator-button:enabled:focus devtools-icon {
          --icon-color: var(--color-text-secondary);
        }
        </style>
      <div class="navigator">
        <div class="navigator-item">
          ${this.createButton({icon: 'ic_undo_16x16_icon', title: i18nString(UIStrings.goBackInAddressHistory),
              event: new HistoryNavigationEvent(Navigation.Backward), enabled: this.canGoBackInHistory})}
          ${this.createButton({icon: 'ic_redo_16x16_icon', title: i18nString(UIStrings.goForwardInAddressHistory),
              event: new HistoryNavigationEvent(Navigation.Forward), enabled: this.canGoForwardInHistory})}
        </div>
        <div class="navigator-item">
          ${this.createButton({icon: 'ic_page_prev_16x16_icon', title: i18nString(UIStrings.previousPage),
              event: new PageNavigationEvent(Navigation.Backward), enabled: true})}
          ${this.createAddressInput()}
          ${this.createButton({icon: 'ic_page_next_16x16_icon', title: i18nString(UIStrings.nextPage),
              event: new PageNavigationEvent(Navigation.Forward), enabled: true})}
        </div>
        ${this.createButton({icon: 'refresh_12x12_icon', title: i18nString(UIStrings.refresh),
            event: new RefreshRequestedEvent(), enabled: true})}
      </div>
      `;
      render(result, this.shadow, {eventContext: this});
    // clang-format on
  }

  private createAddressInput(): LitHtml.TemplateResult {
    const classMap = {
      'address-input': true,
      invalid: !this.valid,
    };
    return html`
      <input class=${LitHtml.Directives.classMap(classMap)} data-input="true" .value=${this.address}
        title=${this.valid ? i18nString(UIStrings.enterAddress) : this.error} @change=${
        this.onAddressChange.bind(this, Mode.Submitted)} @input=${this.onAddressChange.bind(this, Mode.Edit)}/>`;
  }

  private onAddressChange(mode: Mode, event: Event): void {
    const addressInput = event.target as HTMLInputElement;
    this.dispatchEvent(new AddressInputChangedEvent(addressInput.value, mode));
  }

  private createButton(data: {icon: string, title: string, event: Event, enabled: boolean}): LitHtml.TemplateResult {
    const iconColor = data.enabled ? 'var(--color-text-secondary)' : 'var(--color-background-highlight)';
    return html`
      <button class="navigator-button" ?disabled=${!data.enabled}
        data-button=${data.event.type} title=${data.title}
        @click=${this.dispatchEvent.bind(this, data.event)}>
        <devtools-icon .data=${{iconName: data.icon, color: iconColor, width: '14px'} as Components.Icon.IconWithName}>
        </devtools-icon>
      </button>`;
  }
}

customElements.define('devtools-linear-memory-inspector-navigator', LinearMemoryNavigator);

declare global {
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  interface HTMLElementTagNameMap {
    'devtools-linear-memory-inspector-navigator': LinearMemoryNavigator;
  }
}
