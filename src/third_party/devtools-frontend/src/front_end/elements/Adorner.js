// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as i18n from '../i18n/i18n.js';
import * as Platform from '../platform/platform.js';  // eslint-disable-line no-unused-vars
import * as UI from '../ui/ui.js';

import {AdornerCategories} from './AdornerManager.js';

export const UIStrings = {
  /**
  * @description Accessible label for Elements panel adorners. Adorners are small badges/tags
  * displayed next to DOM Elements in the Elements tree. They provide extra information relating to
  * the node at a quick glance.
  */
  adorner: 'adorner',
  /**
  * @description Accessible label for Elements panel adorners. Adorners are small badges/tags
  * displayed next to DOM Elements in the Elements tree. They provide extra information relating to
  * the node at a quick glance. Read by the screen reader when this adorner is currently active.
  */
  adornerActive: 'adorner active',
  /**
  * @description Accessible label for Elements panel adorners. Adorners are small badges/tags
  * displayed next to DOM Elements in the Elements tree. They provide extra information relating to
  * the node at a quick glance. Read by the screen reader when this adorner is focused. The placeholder
  * is the type of this adorner, e.g. grid, flex.
  * @example {grid} PH1
  */
  sAdorner: '{PH1} adorner',
};
const str_ = i18n.i18n.registerUIStrings('elements/Adorner.js', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

const template = document.createElement('template');
template.innerHTML = `
  <style>
    :host {
      display: inline-flex;
    }

    :host(.hidden) {
      display: none;
    }

    :host(.clickable) {
      cursor: pointer;
    }

    :host(:focus) slot {
      border: var(--adorner-border-focus, 1px solid #1a73e8);
    }

    :host([aria-pressed=true]) slot {
      color: var(--adorner-text-color-active, #ffffff);
      background-color: var(--adorner-background-color-active, #1a73e8);
    }

    slot {
      display: inline-flex;
      box-sizing: border-box;
      height: 13px;
      line-height: 13px;
      padding: 0 6px;
      font-size: 8.5px;
      color: var(--adorner-text-color, #3c4043);
      background-color: var(--adorner-background-color, #f1f3f4);
      border: var(--adorner-border, 1px solid #dadce0);
      border-radius: var(--adorner-border-radius, 10px);
    }

    ::slotted(*) {
      height: 10px;
    }
  </style>
  <slot name="content"></slot>
`;

export class Adorner extends HTMLElement {
  /**
   *
   * @param {!HTMLElement} content
   * @param {string} name
   * @param {!{category: (!AdornerCategories|undefined)}} options
   * @return {!Adorner}
   */
  // @ts-ignore typedef TODO(changhaohan): properly type options once this is .ts
  static create(content, name, options = {}) {
    const {category = AdornerCategories.Default} = options;

    const adorner = /** @type {!Adorner} */ (document.createElement('devtools-adorner'));
    content.slot = 'content';
    adorner.append(content);

    adorner.name = name;
    adorner.category = category;

    return adorner;
  }

  constructor() {
    super();

    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.appendChild(template.content.cloneNode(true));

    this.name = '';
    this.category = AdornerCategories.Default;
    this._isToggle = false;
    this._ariaLabelDefault = i18nString(UIStrings.adorner);
    this._ariaLabelActive = i18nString(UIStrings.adornerActive);
  }

  /**
   * @override
   */
  connectedCallback() {
    if (!this.getAttribute('aria-label')) {
      UI.ARIAUtils.setAccessibleName(this, i18nString(UIStrings.sAdorner, {PH1: this.name}));
    }
  }

  /**
   * @return {boolean}
   */
  isActive() {
    return this.getAttribute('aria-pressed') === 'true';
  }

  /**
   * Toggle the active state of the adorner. Optionally pass `true` to force-set
   * an active state; pass `false` to force-set an inactive state.
   * @param {boolean=} forceActiveState
   */
  toggle(forceActiveState) {
    if (!this._isToggle) {
      return;
    }
    const shouldBecomeActive = forceActiveState === undefined ? !this.isActive() : forceActiveState;
    UI.ARIAUtils.setPressed(this, shouldBecomeActive);
    UI.ARIAUtils.setAccessibleName(this, shouldBecomeActive ? this._ariaLabelActive : this._ariaLabelDefault);
  }

  show() {
    this.classList.remove('hidden');
  }

  hide() {
    this.classList.add('hidden');
  }

  /**
   * Make adorner interactive by responding to click events with the provided action
   * and simulating ARIA-capable toggle button behavior.
   * @param {!EventListener} action
   * @param {!{isToggle: (boolean|undefined), shouldPropagateOnKeydown: (boolean|undefined), ariaLabelDefault: (!Platform.UIString.LocalizedString|undefined), ariaLabelActive: (!Platform.UIString.LocalizedString|undefined)}} options
   */
  // @ts-ignore typedef TODO(changhaohan): properly type options once this is .ts
  addInteraction(action, options = {}) {
    const {isToggle = false, shouldPropagateOnKeydown = false, ariaLabelDefault, ariaLabelActive} = options;

    this._isToggle = isToggle;

    if (ariaLabelDefault) {
      this._ariaLabelDefault = ariaLabelDefault;
      UI.ARIAUtils.setAccessibleName(this, ariaLabelDefault);
    }

    if (isToggle) {
      this.addEventListener('click', () => {
        this.toggle();
      });
      if (ariaLabelActive) {
        this._ariaLabelActive = ariaLabelActive;
      }
      this.toggle(false /* initialize inactive state */);
    }

    this.addEventListener('click', action);

    // Simulate an ARIA-capable toggle button
    this.classList.add('clickable');
    UI.ARIAUtils.markAsButton(this);
    this.tabIndex = 0;
    this.addEventListener('keydown', event => {
      if (event.code === 'Enter' || event.code === 'Space') {
        this.click();
        if (!shouldPropagateOnKeydown) {
          event.stopPropagation();
        }
      }
    });
  }
}

self.customElements.define('devtools-adorner', Adorner);
