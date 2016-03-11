// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'cr-shared-menu',

  properties: {
    menuOpen: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
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
  },

  /**
   * The last anchor that was used to open a menu. It's necessary for toggling.
   * @type {?Element}
   */
  lastAnchor_: null,

  /**
   * Adds listeners to the window in order to dismiss the menu on resize and
   * when escape is pressed.
   */
  attached: function() {
    window.addEventListener('resize', this.closeMenu.bind(this));
    window.addEventListener('keydown', function(e) {
      // Escape button on keyboard
      if (e.keyCode == 27)
        this.closeMenu();
    }.bind(this));
  },

  /** Closes the menu. */
  closeMenu: function() {
    this.menuOpen = false;
    this.itemData = null;
  },

  /**
   * Opens the menu at the anchor location.
   * @param {!Element} anchor The location to display the menu.
   * @param {!Object} itemData The contextual item's data.
   */
  openMenu: function(anchor, itemData) {
    this.menuOpen = true;
    this.itemData = itemData;
    this.lastAnchor_ = anchor;

    // Move the menu to the anchor.
    var anchorRect = anchor.getBoundingClientRect();
    var parentRect = this.offsetParent.getBoundingClientRect();

    var left = (isRTL() ? anchorRect.left : anchorRect.right) - parentRect.left;
    var top = anchorRect.top - parentRect.top;

    cr.ui.positionPopupAtPoint(left, top, this, cr.ui.AnchorType.BEFORE);

    // Handle the bottom of the screen.
    if (this.getBoundingClientRect().top != anchorRect.top) {
      var bottom = anchorRect.bottom - parentRect.top;
      cr.ui.positionPopupAtPoint(left, bottom, this, cr.ui.AnchorType.BEFORE);
    }

    this.$.menu.focus();
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
});
