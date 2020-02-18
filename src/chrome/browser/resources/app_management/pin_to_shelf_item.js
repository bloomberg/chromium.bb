// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-pin-to-shelf-item',

  properties: {
    /**
     * @type {App}
     * @private
     */
    app_: Object,

    /**
     * @type {boolean}
     * @private
     */
    hidden: {
      type: Boolean,
      computed: 'isAvailable_(app_)',
      reflectToAttribute: true,
    },

    /**
     * @type {boolean}
     * @private
     */
    disabled: {
      type: Boolean,
      computed: 'isManaged_(app_)',
      reflectToAttribute: true,
    },
  },

  ready: function() {
    // capture the onClick event before it reaches the toggle.
    this.addEventListener('click', this.onClick_, true);
  },

  /**
   * @param {App} app
   * @returns {boolean} true if the app is pinned
   * @private
   */
  getValue_: function(app) {
    if (app === undefined) {
      return false;
    }
    assert(app);
    return app.isPinned === OptionalBool.kTrue;
  },

  /**
   * @param {App} app
   * @returns {boolean} true if pinning is available.
   */
  isAvailable_: function(app) {
    if (app === undefined) {
      return false;
    }
    assert(app);
    return app.hidePinToShelf;
  },

  /**
   * @param {App} app
   * @returns {boolean} true if the pinning is managed by policy.
   * @private
   */
  isManaged_: function(app) {
    if (app === undefined) {
      return false;
    }
    assert(app);
    return app.isPolicyPinned === OptionalBool.kTrue;
  },

  /**
   * @param {Event} event
   * @private
   */
  onClick_: function(event) {
    event.stopPropagation();

    // Disabled
    if (this.isManaged_(this.app_)) {
      return;
    }

    app_management.BrowserProxy.getInstance().handler.setPinned(
        this.app_.id,
        assert(app_management.util.toggleOptionalBool(this.app_.isPinned)),
    );
  },
});
