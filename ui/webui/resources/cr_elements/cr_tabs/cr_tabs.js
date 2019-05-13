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
 *   - left/right changes the tab selection
 *   - space/enter selects the currently focused tab
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
      observer: 'updateSelectionBar_',
    },
  },

  hostAttributes: {
    role: 'tablist',
    tabindex: 0,
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
   * @param {number} index
   * @return {string}
   * @private
   */
  getTabAriaSelected_: function(index) {
    return this.selected == index ? 'true' : 'false';
  },

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getTabCssClass_: function(index) {
    return this.selected == index ? 'selected' : '';
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_: function(e) {
    if (e.key != 'ArrowLeft' && e.key != 'ArrowRight') {
      return;
    }

    e.preventDefault();
    e.stopPropagation();
    const delta =
        e.key == 'ArrowLeft' ? (this.isRtl_ ? 1 : -1) : (this.isRtl_ ? -1 : 1);
    const count = this.tabNames.length;
    this.selected = (count + this.selected + delta) % count;
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
  transformSelectionBar_: function(left, width) {
    const containerWidth = this.offsetWidth;
    const leftPercent = 100 * left / containerWidth;
    const widthRatio = width / containerWidth;
    this.$.selectionBar.style.transform =
        `translateX(${leftPercent}%) scaleX(${widthRatio})`;
  },

  /** @private */
  updateSelectionBar_: function() {
    const selectedTab = this.$$('.selected');
    if (!selectedTab) {
      return;
    }

    this.$.selectionBar.classList.remove('expand', 'contract');
    const {offsetLeft: selectedLeft, offsetWidth: selectedWidth} = selectedTab;
    const oldValue = this.lastSelected_;
    this.lastSelected_ = this.selected;

    // If there is no previously selected tab or the tab has not changed,
    // underline the selected tab instantly.
    if (oldValue == null || oldValue == this.selected) {
      this.transformSelectionBar_(selectedLeft, selectedWidth);
      return;
    }

    // Expand bar to underline the last selected tab, the newly selected tab and
    // everything in between. After expansion is complete, contract bar to
    // underline the selected tab.
    this.$.selectionBar.classList.add('expand');
    this.$.selectionBar.addEventListener('transitionend', () => {
      this.$.selectionBar.classList.replace('expand', 'contract');
      this.transformSelectionBar_(selectedLeft, selectedWidth);
    }, {once: true});

    const {offsetLeft: lastLeft, offsetWidth: lastWidth} =
        this.$$(`.tab:nth-of-type(${oldValue + 1})`);
    const left = Math.min(selectedLeft, lastLeft);
    const right = Math.max(selectedLeft + selectedWidth, lastLeft + lastWidth);
    this.transformSelectionBar_(left, right - left);
  },
});
