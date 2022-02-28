// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-check-element' bundles functionality safety check elements
 * have in common. It is used by all safety check elements: parent, updates,
 * passwors, etc.
 */
import 'chrome://resources/cr_elements/cr_actionable_row_style.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared_css.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * UI states a safety check child can be in. Defines the basic UI of the child.
 */
export enum SafetyCheckIconStatus {
  RUNNING = 0,
  SAFE = 1,
  INFO = 2,
  WARNING = 3,
}

const SettingsSafetyCheckChildElementBase = I18nMixin(PolymerElement);

export class SettingsSafetyCheckChildElement extends
    SettingsSafetyCheckChildElementBase {
  static get is() {
    return 'settings-safety-check-child';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Status of the left hand icon.
       */
      iconStatus: {
        type: Number,
        value: SafetyCheckIconStatus.RUNNING,
      },

      // Primary label of the child.
      label: String,

      // Secondary label of the child.
      subLabel: String,

      // Text of the right hand button. |null| removes it from the DOM.
      buttonLabel: String,

      // Aria label of the right hand button.
      buttonAriaLabel: String,

      // Classes of the right hand button.
      buttonClass: String,

      // Should the entire row be clickable.
      rowClickable: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: 'onRowClickableChanged_',
      },

      // Is the row directed to external link.
      external: {
        type: Boolean,
        value: false,
      },

      rowClickableIcon_: {
        type: String,
        computed: 'computeRowClickableIcon_(external)',
      },

      // Right hand managed icon. |null| removes it from the DOM.
      managedIcon: String,
    };
  }

  iconStatus: SafetyCheckIconStatus;
  label: string;
  subLabel: string;
  buttonLabel: string;
  buttonAriaLabel: string;
  buttonClass: string;
  rowClickable: boolean;
  external: boolean;
  private rowClickableIcon_: string;
  managedIcon: string;

  /** @return The left hand icon for an icon status. */
  private getStatusIcon_(): string|null {
    switch (this.iconStatus) {
      case SafetyCheckIconStatus.RUNNING:
        return null;
      case SafetyCheckIconStatus.SAFE:
        return 'cr:check';
      case SafetyCheckIconStatus.INFO:
        return 'cr:info';
      case SafetyCheckIconStatus.WARNING:
        return 'cr:warning';
      default:
        assertNotReached();
        return null;
    }
  }

  /** @return The left hand icon src for an icon status. */
  private getStatusIconSrc_(): string|null {
    if (this.iconStatus === SafetyCheckIconStatus.RUNNING) {
      return 'chrome://resources/images/throbber_small.svg';
    }
    return null;
  }

  /** @return The left hand icon class for an icon status. */
  private getStatusIconClass_(): string {
    switch (this.iconStatus) {
      case SafetyCheckIconStatus.RUNNING:
      case SafetyCheckIconStatus.SAFE:
        return 'icon-blue';
      case SafetyCheckIconStatus.WARNING:
        return 'icon-red';
      default:
        return '';
    }
  }

  /** @return The left hand icon aria label for an icon status. */
  private getStatusIconAriaLabel_(): string {
    switch (this.iconStatus) {
      case SafetyCheckIconStatus.RUNNING:
        return this.i18n('safetyCheckIconRunningAriaLabel');
      case SafetyCheckIconStatus.SAFE:
        return this.i18n('safetyCheckIconSafeAriaLabel');
      case SafetyCheckIconStatus.INFO:
        return this.i18n('safetyCheckIconInfoAriaLabel');
      case SafetyCheckIconStatus.WARNING:
        return this.i18n('safetyCheckIconWarningAriaLabel');
      default:
        assertNotReached();
        return '';
    }
  }

  /** @return Whether right-hand side button should be shown. */
  private showButton_(): boolean {
    return !!this.buttonLabel;
  }

  private onButtonClick_() {
    this.dispatchEvent(
        new CustomEvent('button-click', {bubbles: true, composed: true}));
  }

  /** @return Whether the right-hand side managed icon should be shown. */
  private showManagedIcon_(): boolean {
    return !!this.managedIcon;
  }

  /** @return The icon to show when the row is clickable. */
  private computeRowClickableIcon_(): string {
    return this.external ? 'cr:open-in-new' : 'cr:arrow-right';
  }

  /** @return The subpage role description if the arrow right icon is used. */
  private getRoleDescription_(): string {
    return this.rowClickableIcon_ === 'cr:arrow-right' ?
        this.i18n('subpageArrowRoleDescription') :
        '';
  }

  private onRowClickableChanged_() {
    // For cr-actionable-row-style.
    this.toggleAttribute('effectively-disabled_', !this.rowClickable);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-check-child': SettingsSafetyCheckChildElement;
  }
}

customElements.define(
    SettingsSafetyCheckChildElement.is, SettingsSafetyCheckChildElement);
