/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';
(() => {
  class CpTextarea extends Polymer.Element {
    static get is() { return 'cp-textarea'; }

    async connectedCallback() {
      super.connectedCallback();
      if (this.autofocus) {
        this.focus();
      }
      this.addEventListener('click', this.onClick_.bind(this));
    }

    async onClick_(event) {
      this.focus();
    }

    get nativeInput() {
      return this.$.input;
    }

    async focus() {
      this.nativeInput.focus();
      while (cp.getActiveElement() !== this.nativeInput) {
        await cp.timeout(50);
        this.nativeInput.focus();
      }
    }

    async blur() {
      this.nativeInput.blur();
      while (cp.getActiveElement() === this.nativeInput) {
        await cp.timeout(50);
        this.nativeInput.blur();
      }
    }

    async onFocus_(event) {
      this.focused = true;
    }

    async onBlur_(event) {
      this.focused = false;
    }

    async onKeyup_(event) {
      this.value = event.target.value;
    }
  }

  CpTextarea.properties = {
    autofocus: {type: Boolean},
    focused: {
      type: Boolean,
      reflectToAttribute: true,
    },
    label: {type: String},
    value: {type: String},
  };

  customElements.define(CpTextarea.is, CpTextarea);
})();
