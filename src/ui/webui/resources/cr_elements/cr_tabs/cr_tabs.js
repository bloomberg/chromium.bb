// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-tabs' is a control used for selecting different sections or
 * tabs. cr-tabs was created to replace paper-tabs and paper-tab. cr-tabs
 * displays the name of each tab provided by |tabs|. A 'selected-changed' event
 * is fired any time |selected| is changed.
 *
 * cr-tabs takes its #selectionBar animation from paper-tabs.
 *
 * Keyboard behavior
 *   - Home, End, ArrowLeft and ArrowRight changes the tab selection
 *
 * Known limitations
 *   - no "disabled" state for the cr-tabs as a whole or individual tabs
 *   - cr-tabs does not accept any <slot> (not necessary as of this writing)
 *   - no horizontal scrolling, it is assumed that tabs always fit in the
 *     available space
 */
Polymer({
  is: 'cr-tabs',

  properties: {
    /**
     * Tab names displayed in each tab.
     * @type {!Array<string>}
     */
    tabNames: {
      type: Array,
      value: () => [],
    },

    /** Index of the selected tab. */
    selected: {
      type: Number,
      notify: true,
      observer: 'updateUi_',
    },
  },

  hostAttributes: {
    role: 'tablist',
  },

  listeners: {
    keydown: 'onKeyDown_',
  },

  /** @private {boolean} */
  isRtl_: false,

  /** @private {?number} */
  lastSelected_: null,

  /** @override */
  attached: function() {
    this.isRtl_ = this.matches(':host-context([dir=rtl]) cr-tabs');
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_: function(e) {
    const count = this.tabNames.length;
    let newSelection;
    if (e.key == 'Home') {
      newSelection = 0;
    } else if (e.key == 'End') {
      newSelection = count - 1;
    } else if (e.key == 'ArrowLeft' || e.key == 'ArrowRight') {
      const delta = e.key == 'ArrowLeft' ? (this.isRtl_ ? 1 : -1) :
                                           (this.isRtl_ ? -1 : 1);
      newSelection = (count + this.selected + delta) % count;
    } else {
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    this.selected = newSelection;
  },

  /**
   * @param {!{model: !{index: number}}} _
   * @private
   */
  onTabClick_: function({model: {index}}) {
    this.selected = index;
  },

  /**
   * @param {number} left
   * @param {number} width
   * @private
   */
  updateSelectionBar_: function(left, width) {
    const containerWidth = this.offsetWidth;
    const leftPercent = 100 * left / containerWidth;
    const widthRatio = width / containerWidth;
    this.$.selectionBar.style.transform =
        `translateX(${leftPercent}%) scaleX(${widthRatio})`;
  },

  /** @private */
  updateUi_: function() {
    const tabs = this.shadowRoot.querySelectorAll('.tab');
    // Tabs are not rendered yet by dom-repeat. Skip this update since
    // dom-repeat will fire a dom-change event when it is ready.
    if (tabs.length == 0) {
      return;
    }

    tabs.forEach((tab, i) => {
      const isSelected = this.selected == i;
      if (isSelected) {
        tab.focus();
      }
      tab.classList.toggle('selected', isSelected);
      tab.setAttribute('aria-selected', isSelected);
      tab.setAttribute('tabindex', isSelected ? 0 : -1);
    });

    if (this.selected == undefined) {
      return;
    }

    this.$.selectionBar.classList.remove('expand', 'contract');
    const oldValue = this.lastSelected_;
    this.lastSelected_ = this.selected;

    // If there is no previously selected tab or the tab has not changed,
    // underline the selected tab instantly.
    if (oldValue == null || oldValue == this.selected) {
      // When handling the initial 'dom-change' event, it's possible for the
      // selected tab to exist and not yet be fully rendered. This will result
      // in the selection bar not rendering correctly.
      setTimeout(() => {
        const {offsetLeft, offsetWidth} = tabs[this.selected];
        this.updateSelectionBar_(offsetLeft, offsetWidth);
      });
      return;
    }

    const {offsetLeft: newLeft, offsetWidth: newWidth} = tabs[this.selected];
    const {offsetLeft: oldLeft, offsetWidth: oldWidth} = tabs[oldValue];

    // Expand bar to underline the last selected tab, the newly selected tab and
    // everything in between. After expansion is complete, contract bar to
    // underline the selected tab.
    this.$.selectionBar.classList.add('expand');
    this.$.selectionBar.addEventListener('transitionend', () => {
      this.$.selectionBar.classList.replace('expand', 'contract');
      this.updateSelectionBar_(newLeft, newWidth);
    }, {once: true});

    const left = Math.min(newLeft, oldLeft);
    const right = Math.max(newLeft + newWidth, oldLeft + oldWidth);
    this.updateSelectionBar_(left, right - left);
  },
});
