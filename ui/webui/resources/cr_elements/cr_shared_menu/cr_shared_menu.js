// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Same as paper-menu-button's custom easing cubic-bezier param. */
var SLIDE_CUBIC_BEZIER = 'cubic-bezier(0.3, 0.95, 0.5, 1)';

Polymer({
  is: 'cr-shared-menu',

  behaviors: [Polymer.IronA11yKeysBehavior],

  properties: {
    menuOpen: {
      type: Boolean,
      observer: 'menuOpenChanged_',
      value: false,
      notify: true,
    },

    /**
     * The contextual item that this menu was clicked for.
     *  e.g. the data used to render an item in an <iron-list> or <dom-repeat>
     * @type {?Object}
     */
    itemData: {
      type: Object,
      value: null,
    },

    /** @override */
    keyEventTarget: {
      type: Object,
      value: function() {
        return this.$.menu;
      }
    },

    openAnimationConfig: {
      type: Object,
      value: function() {
        return [{
          name: 'fade-in-animation',
          timing: {
            delay: 50,
            duration: 200
          }
        }, {
          name: 'paper-menu-grow-width-animation',
          timing: {
            delay: 50,
            duration: 150,
            easing: SLIDE_CUBIC_BEZIER
          }
        }, {
          name: 'paper-menu-grow-height-animation',
          timing: {
            delay: 100,
            duration: 275,
            easing: SLIDE_CUBIC_BEZIER
          }
        }];
      }
    },

    closeAnimationConfig: {
      type: Object,
      value: function() {
        return [{
          name: 'fade-out-animation',
          timing: {
            duration: 150
          }
        }];
      }
    }
  },

  keyBindings: {
    'tab': 'onTabPressed_',
  },

  listeners: {
    'dropdown.iron-overlay-canceled': 'onOverlayCanceled_',
  },

  /**
   * The last anchor that was used to open a menu. It's necessary for toggling.
   * @private {?Element}
   */
  lastAnchor_: null,

  /**
   * The first focusable child in the menu's light DOM.
   * @private {?Element}
   */
  firstFocus_: null,

  /**
   * The last focusable child in the menu's light DOM.
   * @private {?Element}
   */
  lastFocus_: null,

  /** @override */
  attached: function() {
    window.addEventListener('resize', this.closeMenu.bind(this));
  },

  /** Closes the menu. */
  closeMenu: function() {
    if (this.root.activeElement == null) {
      // Something else has taken focus away from the menu. Do not attempt to
      // restore focus to the button which opened the menu.
      this.$.dropdown.restoreFocusOnClose = false;
    }
    this.menuOpen = false;
  },

  /**
   * Opens the menu at the anchor location.
   * @param {!Element} anchor The location to display the menu.
   * @param {!Object} itemData The contextual item's data.
   */
  openMenu: function(anchor, itemData) {
    if (this.lastAnchor_ == anchor && this.menuOpen)
      return;

    if (this.menuOpen)
      this.closeMenu();

    this.itemData = itemData;
    this.lastAnchor_ = anchor;
    this.$.dropdown.restoreFocusOnClose = true;

    var focusableChildren = Polymer.dom(this).querySelectorAll(
        '[tabindex]:not([hidden]),button:not([hidden])');
    if (focusableChildren.length > 0) {
      this.$.dropdown.focusTarget = focusableChildren[0];
      this.firstFocus_ = focusableChildren[0];
      this.lastFocus_ = focusableChildren[focusableChildren.length - 1];
    }

    // Move the menu to the anchor.
    this.$.dropdown.positionTarget = anchor;
    this.menuOpen = true;
  },

  /**
   * Toggles the menu for the anchor that is passed in.
   * @param {!Element} anchor The location to display the menu.
   * @param {!Object} itemData The contextual item's data.
   */
  toggleMenu: function(anchor, itemData) {
    if (anchor == this.lastAnchor_ && this.menuOpen)
      this.closeMenu();
    else
      this.openMenu(anchor, itemData);
  },

  /**
   * Trap focus inside the menu. As a very basic heuristic, will wrap focus from
   * the first element with a nonzero tabindex to the last such element.
   * TODO(tsergeant): Use iron-focus-wrap-behavior once it is available
   * (https://github.com/PolymerElements/iron-overlay-behavior/issues/179).
   * @param {CustomEvent} e
   */
  onTabPressed_: function(e) {
    if (!this.firstFocus_ || !this.lastFocus_)
      return;

    var toFocus;
    var keyEvent = e.detail.keyboardEvent;
    if (keyEvent.shiftKey && keyEvent.target == this.firstFocus_)
      toFocus = this.lastFocus_;
    else if (keyEvent.target == this.lastFocus_)
      toFocus = this.firstFocus_;

    if (!toFocus)
      return;

    e.preventDefault();
    toFocus.focus();
  },

  /**
   * Ensure the menu is reset properly when it is closed by the dropdown (eg,
   * clicking outside).
   * @private
   */
  menuOpenChanged_: function() {
    if (!this.menuOpen) {
      this.itemData = null;
      this.lastAnchor_ = null;
    }
  },

  /**
   * Prevent focus restoring when tapping outside the menu. This stops the
   * focus moving around unexpectedly when closing the menu with the mouse.
   * @param {CustomEvent} e
   * @private
   */
  onOverlayCanceled_: function(e) {
    if (e.detail.type == 'tap')
      this.$.dropdown.restoreFocusOnClose = false;
  },
});
