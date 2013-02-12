/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.require('ui');
base.require('color_mappings');
base.require('quad_view');

base.exportTo('ccfv', function() {

  /**
   * @constructor
   */
  function TreeQuadViewSelection(view, tiles) {
    this.view = view;
    this.tiles = tiles;
  }

  TreeQuadViewSelection.prototype = {
    activate: function() {
      this.tiles.forEach(function(tile) {
        tile.selected = true;
      });
      this.view.updateQuads_();
    },

    deactivate: function() {
      this.tiles.forEach(function(tile) {
        tile.selected = false;
      });
      this.view.updateQuads_();
    }
  };

  /**
   * @constructor
   */
  var TreeQuadView = ui.define(ccfv.QuadView);

  TreeQuadView.prototype = {
    __proto__: ccfv.QuadView.prototype,

    decorate: function() {
      this.which_tree_ = undefined;
      this.tiles_ = undefined;
      this.colorMap_ = ccfv.ALL_TILE_COLOR_MAPS[0];
    },

    get tiles() {
      return this.tiles_;
    },

    set tiles(tiles) {
      this.tiles_ = tiles;
      this.updateQuads_();
    },

    get which_tree() {
      return this.which_tree_;
    },

    set which_tree(which_tree) {
      this.which_tree_ = which_tree;
      this.updateQuads_();
    },

    get colorMap() {
      return this.colorMap_;
    },

    set colorMap(map) {
      this.colorMap_ = map;
      this.updateQuads_();
    },

    updateQuads_: function() {
      if (this.which_tree_ === undefined ||
          !this.tiles_) {
        this.quads = undefined;
        return;
      }

      var tiles = this.tiles_;
      var which_tree = this.which_tree_;

      var quads = [];
      for (var i = 0; i < tiles.length; i++) {
        var tile = tiles[i];
        var priority = tile.priority[which_tree];
        if (!priority.is_live)
          continue;

        var value = this.colorMap_.getValueForTile(tile, priority);
        var backgroundColor = value !== undefined ?
          this.colorMap_.getBackgroundColorForValue(value) : undefined;

        var quad = priority.current_screen_quad;
        quads.push({
          backgroundColor: backgroundColor,
          selected: tile.selected,
          p1: quad.p1,
          p2: quad.p2,
          p3: quad.p3,
          p4: quad.p4
        });
      }
      this.quads = quads;
    },

    createSelection_: function(quadIndices) {
      var tiles = [];
      for (var i = 0; i < quadIndices.length; i++)
        tiles.push(this.tiles_[quadIndices[i]]);
      return new TreeQuadViewSelection(this, tiles);
    }
  }

  return {
    TreeQuadView: TreeQuadView,
    TreeQuadViewSelection: TreeQuadViewSelection,
  }
});

