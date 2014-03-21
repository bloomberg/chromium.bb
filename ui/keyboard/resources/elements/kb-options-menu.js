// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('kb-options-menu', {
});

Polymer('kb-options-menu-item', {
  down: function(event) {
    event.stopPropagation();
  },

  over: function(event) {
    this.classList.add('active');
  },

  out: function(event) {
    this.classList.remove('active');
  },

  up: function(event) {
    this.hidden = true;
    hideKeyboard();
  },
});

(function() {
  /**
   * The lock and unlock label for this key.
   * @const
   * @enum {string}
   */
  // TODO(bshe): Localize the string resources. http://crbug.com/328871
  var KEY_LABEL = {
    LOCK: 'Lock',
    UNLOCK: 'Unlock'
  };

  Polymer('kb-options-menu-toggle-lock-item', {
    up: function(event) {
      lockKeyboard(!keyboardLocked());
      this.hidden = true;
    },

    get label() {
      return keyboardLocked() ? KEY_LABEL.UNLOCK : KEY_LABEL.LOCK;
    }
  });
})();

Polymer('kb-keyboard-overlay', {
  up: function(event) {
    this.hidden = true;
    this.classList.remove('arm-dismiss');
    event.stopPropagation();
  },

  down: function(event) {
    this.classList.add('arm-dismiss');
    event.stopPropagation();
  },

  hiddenChanged: function() {
    this.fire('stateChange', {
      state: 'overlayVisibility',
      value: !!this.hidden
    });
  }
});
