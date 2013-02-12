/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.require('ui');
base.requireStylesheet('tile_view');
base.exportTo('ccfv', function() {
  var TileView = ui.define('x-tile-view');
  TileView.prototype = {
    __proto__: HTMLUnknownElement.prototype,

    decorate: function() {
      this.tile_ = undefined;
    },

    set tile(tile) {
      this.tile_ = tile;
      this.updateChildren_();
    },

    updateChildren_: function() {
      this.textContent = '';
      if (!this.tile_)
        return;

      var simpleObj = {};
      this.tile_.dumpToSimpleObject(simpleObj);
      this.textContent = JSON.stringify(
        simpleObj,
        null,
        2);
    }
  };

  return {
    TileView: TileView
  }
});

