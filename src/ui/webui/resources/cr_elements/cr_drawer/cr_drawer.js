// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../shared_vars_css.m.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertNotReached} from '../../js/assert.m.js';
import {listenOnce} from '../../js/util.m.js';

/** @polymer */
export class CrDrawerElement extends PolymerElement {
  static get is() {
    return 'cr-drawer';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      heading: String,

      /** @private */
      show_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** The alignment of the drawer on the screen ('ltr' or 'rtl'). */
      align: {
        type: String,
        value: 'ltr',
        reflectToAttribute: true,
      },

      /**
       * An iron-icon resource name, e.g. "cr20:menu". If null, no icon will
       * be shown.
       */
      iconName: {
        type: String,
        value: null,
      },

      /** Title attribute for the icon, if shown. */
      iconTitle: String,
    };
  }

  /**
   * @param {string} eventName
   * @param {*=} detail
   * @private
   */
  fire_(eventName, detail) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  /** @type {boolean} */
  get open() {
    return this.$.dialog.open;
  }

  set open(value) {
    assertNotReached('Cannot set |open|.');
  }

  /** Toggles the drawer open and close. */
  toggle() {
    if (this.open) {
      this.cancel();
    } else {
      this.openDrawer();
    }
  }

  /** Shows drawer and slides it into view. */
  openDrawer() {
    if (this.open) {
      return;
    }
    this.$.dialog.showModal();
    this.show_ = true;
    this.fire_('cr-drawer-opening');
    listenOnce(this.$.dialog, 'transitionend', () => {
      this.fire_('cr-drawer-opened');
    });
  }

  /**
   * Slides the drawer away, then closes it after the transition has ended. It
   * is up to the owner of this component to differentiate between close and
   * cancel.
   * @param {boolean} cancel
   */
  dismiss_(cancel) {
    if (!this.open) {
      return;
    }
    this.show_ = false;
    listenOnce(this.$.dialog, 'transitionend', () => {
      this.$.dialog.close(cancel ? 'canceled' : 'closed');
    });
  }

  cancel() {
    this.dismiss_(true);
  }

  close() {
    this.dismiss_(false);
  }

  /** @return {boolean} */
  wasCanceled() {
    return !this.open && this.$.dialog.returnValue === 'canceled';
  }

  /**
   * Handles a tap on the (optional) icon.
   * @param {!Event} event
   * @private
   */
  onIconTap_(event) {
    this.cancel();
  }

  /**
   * Stop propagation of a tap event inside the container. This will allow
   * |onDialogTap_| to only be called when clicked outside the container.
   * @param {!Event} event
   * @private
   */
  onContainerTap_(event) {
    event.stopPropagation();
  }

  /**
   * Close the dialog when tapped outside the container.
   * @private
   */
  onDialogTap_() {
    this.cancel();
  }

  /**
   * Overrides the default cancel machanism to allow for a close animation.
   * @param {!Event} event
   * @private
   */
  onDialogCancel_(event) {
    event.preventDefault();
    this.cancel();
  }

  /**
   * @param {!Event} event
   * @private
   */
  onDialogClose_(event) {
    // Catch and re-fire the 'close' event such that it bubbles across Shadow
    // DOM v1.
    this.fire_('close');
  }
}

customElements.define(CrDrawerElement.is, CrDrawerElement);
