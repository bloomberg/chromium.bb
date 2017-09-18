// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /**
   * The class name to set on the document element.
   * @const
   */
  var CLASS_NAME = 'focus-outline-visible';

  /**
   * This class sets a CSS class name on the HTML element of |doc| when the user
   * presses the tab key. It removes the class name when the user clicks
   * anywhere.
   *
   * This allows you to write CSS like this:
   *
   * html.focus-outline-visible my-element:focus {
   *   outline: 5px auto -webkit-focus-ring-color;
   * }
   *
   * And the outline will only be shown if the user uses the keyboard to get to
   * it.
   *
   * @param {Document} doc The document to attach the focus outline manager to.
   * @constructor
   */
  function FocusOutlineManager(doc) {
    this.classList_ = doc.documentElement.classList;

    var self = this;

    doc.addEventListener('keydown', function(e) {
      self.focusByKeyboard_ = true;
    }, true);

    doc.addEventListener('mousedown', function(e) {
      self.focusByKeyboard_ = false;
    }, true);

    doc.addEventListener('focus', function(event) {
      // Update visibility only when focus is actually changed.
      self.updateVisibility();
    }, true);

    doc.addEventListener('focusout', function(event) {
      window.setTimeout(function() {
        if (!doc.hasFocus()) {
          self.focusByKeyboard_ = true;
          self.updateVisibility();
        }
      }, 0);
    });

    this.updateVisibility();
  }

  FocusOutlineManager.prototype = {
    /**
     * Whether focus change is triggered by TAB key.
     * @type {boolean}
     * @private
     */
    focusByKeyboard_: true,

    updateVisibility: function() {
      this.visible = this.focusByKeyboard_;
    },

    /**
     * Whether the focus outline should be visible.
     * @type {boolean}
     */
    set visible(visible) {
      this.classList_.toggle(CLASS_NAME, visible);
    },
    get visible() {
      return this.classList_.contains(CLASS_NAME);
    }
  };

  /** @type {!Map<!Document, !cr.ui.FocusOutlineManager>} */
  var docsToManager = new Map();

  /**
   * Gets a per document singleton focus outline manager.
   * @param {!Document} doc The document to get the |FocusOutlineManager| for.
   * @return {!cr.ui.FocusOutlineManager} The per document singleton focus
   *     outline manager.
   */
  FocusOutlineManager.forDocument = function(doc) {
    var manager = docsToManager.get(doc);
    if (!manager) {
      manager = new FocusOutlineManager(doc);
      docsToManager.set(doc, manager);
    }
    return manager;
  };

  return {FocusOutlineManager: FocusOutlineManager};
});
