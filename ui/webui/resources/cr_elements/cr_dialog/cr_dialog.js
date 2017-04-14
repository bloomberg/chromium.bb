// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-dialog' is a component for showing a modal dialog. If the
 * dialog is closed via close(), a 'close' event is fired. If the dialog is
 * canceled via cancel(), a 'cancel' event is fired followed by a 'close' event.
 * Additionally clients can inspect the dialog's |returnValue| property inside
 * the 'close' event listener to determine whether it was canceled or just
 * closed, where a truthy value means success, and a falsy value means it was
 * canceled.
 */
Polymer({
  is: 'cr-dialog',
  extends: 'dialog',

  properties: {
    /**
     * Alt-text for the dialog close button.
     */
    closeText: String,

    /**
     * True if the dialog should remain open on 'popstate' events. This is used
     * for navigable dialogs that have their separate navigation handling code.
     */
    ignorePopstate: {
      type: Boolean,
      value: false,
    },

    /**
     * True if the dialog should ignore 'Enter' keypresses.
     */
    ignoreEnterKey: {
      type: Boolean,
      value: false,
    },

    showScrollBorders: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  /** @private {?IntersectionObserver} */
  intersectionObserver_: null,

  /** @private {?MutationObserver} */
  mutationObserver_: null,

  /** @override */
  ready: function() {
    // If the active history entry changes (i.e. user clicks back button),
    // all open dialogs should be cancelled.
    window.addEventListener('popstate', function() {
      if (!this.ignorePopstate && this.open)
        this.cancel();
    }.bind(this));

    if (!this.ignoreEnterKey)
      this.addEventListener('keypress', this.onKeypress_.bind(this));
  },

  /** @override */
  attached: function() {
    if (!this.showScrollBorders)
      return;

    this.mutationObserver_ = new MutationObserver(function() {
      if (this.open) {
        this.addIntersectionObserver_();
      } else {
        this.removeIntersectionObserver_();
      }
    }.bind(this));

    this.mutationObserver_.observe(this, {
      attributes: true,
      attributeFilter: ['open'],
    });
  },

  /** @override */
  detached: function() {
    this.removeIntersectionObserver_();
    if (this.mutationObserver_) {
      this.mutationObserver_.disconnect();
      this.mutationObserver_ = null;
    }
  },

  /** @private */
  addIntersectionObserver_: function() {
    if (this.intersectionObserver_)
      return;

    var bodyContainer = this.$$('.body-container');

    var bottomMarker = this.$.bodyBottomMarker;
    var topMarker = this.$.bodyTopMarker;

    var callback = function(entries) {
      assert(entries.length <= 2);
      for (var i = 0; i < entries.length; i++) {
        var target = entries[i].target;
        assert(target == bottomMarker || target == topMarker);

        var classToToggle =
            target == bottomMarker ? 'bottom-scrollable' : 'top-scrollable';

        bodyContainer.classList.toggle(
            classToToggle, entries[i].intersectionRatio == 0);
      }
    };

    this.intersectionObserver_ = new IntersectionObserver(
        callback,
        /** @type {IntersectionObserverInit} */ ({
          root: bodyContainer,
          threshold: 0,
        }));
    this.intersectionObserver_.observe(bottomMarker);
    this.intersectionObserver_.observe(topMarker);
  },

  /** @private */
  removeIntersectionObserver_: function() {
    if (this.intersectionObserver_) {
      this.intersectionObserver_.disconnect();
      this.intersectionObserver_ = null;
    }
  },

  cancel: function() {
    this.fire('cancel');
    HTMLDialogElement.prototype.close.call(this, '');
  },

  /**
   * @param {string=} opt_returnValue
   * @override
   */
  close: function(opt_returnValue) {
    HTMLDialogElement.prototype.close.call(this, 'success');
  },

  /** @return {!PaperIconButtonElement} */
  getCloseButton: function() {
    return this.$.close;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onKeypress_: function(e) {
    if (e.key != 'Enter')
      return;

    // Accept Enter keys from either the dialog, or a child paper-input element.
    if (e.target != this && e.target.tagName != 'PAPER-INPUT')
      return;

    var actionButton =
        this.querySelector('.action-button:not([disabled]):not([hidden])');
    if (actionButton) {
      actionButton.click();
      e.preventDefault();
    }
  },
});
