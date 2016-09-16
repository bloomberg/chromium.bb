// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * cr-lazy-render is a simple variant of dom-if designed for lazy rendering
 * of elements that are accessed imperatively.
 * Usage:
 *   <template is="cr-lazy-render" id="menu">
 *     <heavy-menu></heavy-menu>
 *   </template>
 *
 *   this.$.menu.get().then(function(menu) {
 *     menu.show();
 *   });
 */

Polymer({
  is: 'cr-lazy-render',
  extends: 'template',

  behaviors: [
    Polymer.Templatizer
  ],

  /** @private {Promise<Element>} */
  renderPromise_: null,

  /** @private {TemplatizerNode} */
  child_: null,

  /**
   * Stamp the template into the DOM tree asynchronously
   * @return {Promise<Element>} Promise which resolves when the template has
   *   been stamped.
   */
  get: function() {
    if (!this.renderPromise_) {
      this.renderPromise_ = new Promise(function(resolve) {
        this._debounceTemplate(function() {
          this.render_();
          this.renderPromise_ = null;
          resolve(this.getIfExists());
        }.bind(this));
      }.bind(this));
    }
    return this.renderPromise_;
  },

  /**
   * @return {?Element} The element contained in the template, if it has
   *   already been stamped.
   */
  getIfExists: function() {
    return this.child_;
  },

  /** @private */
  render_: function() {
    if (!this.ctor)
      this.templatize(this);
    var parentNode = this.parentNode;
    if (parentNode && !this.child_) {
      var instance = this.stamp({});
      this.child_ = instance.root.firstElementChild;
      parentNode.insertBefore(instance.root, this);
    }
  },

  /**
   * @param {string} prop
   * @param {Object} value
   */
  _forwardParentProp: function(prop, value) {
    if (this.child_)
      this.child_._templateInstance[prop] = value;
  },

  /**
   * @param {string} path
   * @param {Object} value
   */
  _forwardParentPath: function(path, value) {
    if (this.child_)
      this.child_._templateInstance.notifyPath(path, value, true);
  }
});
