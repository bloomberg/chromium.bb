/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * @fileoverview
 * `cr-collapse` creates a collapsible block of content. By default, the content
 * will be collapsed. Use `opened` or `toggle()` to show/hide the content.
 * `cr-collapse` adjusts the height/width of the collapsible element to
 * show/hide the content. So avoid putting padding/margin/border on the
 * collapsible directly, and instead put a `div` inside and style that.
 *
 * When a `cr-collapse` is toggled, its `opened` field changes immediately, but
 * its show/hide animation finishes some time afterward. To determine if the
 * element is actually shown in the UI, check for the `cr-collapse-closed` CSS
 * class.
 *
 * Example:
 *     <style>
 *       #collapse-content {
 *         padding: 15px;
 *       }
 *     </style>
 *     <button on-click="{{toggle}}">toggle collapse</button>
 *     <cr-collapse id="collapse">
 *       <div id="collapse-content">
 *         Content goes here...
 *       </div>
 *     </cr-collapse>
 *
 *     ...
 *
 *     toggle: function() {
 *       this.$.collapse.toggle();
 *     }
 *
 * @element cr-collapse
 */
Polymer({
  publish: {
    /**
     * Set opened to `true` to show the collapse element and to `false` to hide
     * it.
     *
     * @attribute opened
     * @type boolean
     * @default false
     */
    opened: {value: false, reflect: true},
  },

  handleResize_: function() {
    this.classList.toggle('cr-collapse-closed', !this.opened);
  },

  toggle: function() {
    this.$.collapse.toggle();
  },

  ready: function() {
    this.$.collapse.addEventListener(
        'core-resize', this.handleResize_.bind(this));
    this.handleResize_();
  },
});
