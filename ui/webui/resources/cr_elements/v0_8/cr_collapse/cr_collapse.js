// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `cr-collapse` creates a collapsible block of content. By default, the content
 * will be collapsed. Use `opened` or `toggle()` to show/hide the content.
 * `cr-collapse` adjusts the height/width of the collapsible element to
 * show/hide the content. So avoid putting padding/margin/border on the
 * collapsible directly, and instead put a `div` inside and style that.
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
  is: 'cr-collapse',

  properties: {
    /**
     * Set opened to `true` to show the collapse element and to `false` to hide
     * it.
     */
    opened: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      notify: true
    },
  },

  toggle: function() {
    this.$.collapse.toggle();
  },
});
