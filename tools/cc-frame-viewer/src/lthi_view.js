/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.require('ui');
base.require('ui.list_view');
base.require('quad_view_viewport');
base.require('tree_quad_view');
base.requireStylesheet('lthi_view');

base.exportTo('ccfv', function() {

  var TreeQuadView = ccfv.TreeQuadView;

  var LTHIView = ui.define('x-lthi-view');

  LTHIView.prototype = {
    __proto__: HTMLUnknownElement.prototype,

    decorate: function() {
      this.lthi_ = undefined;
      this.trees_ = document.createElement('x-trees');

      this.viewport_ = undefined;

      var optionsEl = document.createElement('x-frame-view-options');

      this.colorMapSelector_ = document.createElement('select');
      this.colorMapSelector_.addEventListener(
        'change', this.onColorMapChanged_.bind(this));
      ccfv.ALL_TILE_COLOR_MAPS.forEach(function(colorMap) {
        var mapOption = document.createElement('option');
        mapOption.textContent = colorMap.title;
        mapOption.colorMap = colorMap;
        this.colorMapSelector_.appendChild(mapOption);
      }.bind(this));
      optionsEl.appendChild(ui.createSpan('Color map:'));
      optionsEl.appendChild(this.colorMapSelector_);

      this.scaleSelector_ = document.createElement('select');
      this.scaleSelector_.addEventListener(
        'change', this.onScaleChanged_.bind(this));
      [1/32., 1/16., 1/8., 1/4, 1/2, 1].forEach(function(scale) {
        var mapOption = document.createElement('option');
        mapOption.textContent = Math.floor(scale * 100) + '%';
        mapOption.scale = scale;
        this.scaleSelector_.appendChild(mapOption);
      }.bind(this));
      this.scaleSelector_.selectedIndex = 3;
      optionsEl.appendChild(ui.createSpan('Scale:'));
      optionsEl.appendChild(this.scaleSelector_);

      this.headerTextEl_ = document.createElement('span');
      optionsEl.appendChild(ui.createSpan('Details:'));
      optionsEl.appendChild(this.headerTextEl_);


      this.activeTreeQuadView_ = new TreeQuadView();
      this.pendingTreeQuadView_ = new TreeQuadView();
      this.inactiveTileView_ = new InactiveTileView();

      this.onSelectionChanged_ = this.onSelectionChanged_.bind(this);
      this.activeTreeQuadView_.addEventListener('selection-changed',
                                                this.onSelectionChanged_);
      this.pendingTreeQuadView_.addEventListener('selection-changed',
                                                 this.onSelectionChanged_);
      this.inactiveTileView_.addEventListener('selection-changed',
                                              this.onSelectionChanged_);

      this.trees_.appendChild(this.activeTreeQuadView_);
      this.trees_.appendChild(this.pendingTreeQuadView_);
      this.appendChild(optionsEl);
      this.appendChild(this.trees_);
      this.appendChild(this.inactiveTileView_);
    },

    get lthi() {
      return this.lthi_;
    },

    set lthi(lthi) {
      this.lthi_ = lthi;
      this.updateChildren_();
    },

    onColorMapChanged_: function() {
      this.activeTreeQuadView_.colorMap =
        this.colorMapSelector_.selectedOptions[0].colorMap;
      this.pendingTreeQuadView_.colorMap =
        this.colorMapSelector_.selectedOptions[0].colorMap;
    },

    onScaleChanged_: function() {
      if (this.viewport_.scale)
        this.viewport_.scale = this.scaleSelector_.selectedOptions[0].scale;
    },

    updateChildren_: function() {
      var lthi = this.lthi_;
      if (!lthi) {
        this.activeTreeQuadView_.tree = undefined;
        this.pendingTreeQuadView_.tree = undefined;
        this.viewport_ = undefined;
        return;
      }

      var bbox = lthi.history.allTilesBBox;
      if (!this.viewport_) {
        if (!bbox.isEmpty) {
          this.viewport_ = new ccfv.QuadViewViewport(
            bbox);
          this.viewport_.scale = this.scaleSelector_.selectedOptions[0].scale;
          this.activeTreeQuadView_.viewport = this.viewport_;
          this.pendingTreeQuadView_.viewport = this.viewport_;
        } else {
          return;
        }
      }

      this.activeTreeQuadView_.which_tree = lthi.activeTree.which_tree;
      this.activeTreeQuadView_.tiles = lthi.activeTree.tiles;
      this.activeTreeQuadView_.headerText =
        'Active Tree: ' + lthi.activeTree.tiles.length + ' live tiles';
      this.activeTreeQuadView_.deviceViewportSizeForFrame =
        lthi.deviceViewportSize;

      this.pendingTreeQuadView_.which_tree = lthi.pendingTree.which_tree;
      this.pendingTreeQuadView_.tiles = lthi.pendingTree.tiles;
      this.pendingTreeQuadView_.headerText =
        'Pending Tree: ' + lthi.pendingTree.tiles.length + ' live tiles';
      this.pendingTreeQuadView_.deviceViewportSizeForFrame =
        lthi.deviceViewportSize;

      this.headerTextEl_.textContent = lthi.inactiveTiles.length +
        ' inactive tiles';
      this.inactiveTileView_.tiles = lthi.inactiveTiles;
    },

    onSelectionChanged_: function(e) {
      var e2 = new base.Event('selection-changed');
      e2.selection = e.selection;
      this.dispatchEvent(e2);
      return true;
    }
  };

  function InactiveTileViewSelection(view, tiles) {
    this.view = view;
    this.tiles = tiles;
  }

  InactiveTileViewSelection.prototype = {
    activate: function() {
      this.tiles.forEach(function(tile) {
        tile.selected = true;
      });
      this.quad_view.viewport.didTileSelectednessChange();
      try {
        this.view.ignoreChangeEvents_ = true;
        this.view.selectedTile = this.tiles[0];
      } finally {
        this.view.ignoreChangeEvents_ = false;
      }
    },

    deactivate: function() {
      this.tiles.forEach(function(tile) {
        tile.selected = false;
      });
      this.quad_view.viewport.didTileSelectednessChange();
      try {
        this.view.ignoreChangeEvents_ = true;
        this.view.selectedElement = undefined;
      } finally {
        this.view.ignoreChangeEvents_ = false;
      }
    }
  }

  var InactiveTileView = ui.define('x-inactive-tile-view');

  InactiveTileView.prototype = {
    __proto__: HTMLUnknownElement.prototype,

    decorate: function() {
      this.viewport_ = undefined;
      this.tiles_ = undefined;
      this.header_ = document.createElement('div');
      this.tileList_ = new ui.ListView();
      this.ignoreChangeEvents_ = false;
      this.tileList_.addEventListener(
        'selection-changed',
        this.onSelectionChanged_.bind(this));
      this.appendChild(this.header_);
      this.appendChild(this.tileList_);
    },

    set viewport(viewport) {
      this.viewport_ = viewport;
      this.updateChildren_();
    },

    get viewport() {
      return this.viewport_;
    },

    set tiles(tiles) {
      this.tiles_ = tiles;
      this.updateChildren_();
    },

    get tiles() {
      return this.tiles_;
    },

    set selectedTile(tile) {
      for (var i = 0; i < this.tileList.children.length; i++) {
        if(this.tileList.children[i].tile == tile) {
          this.tileList.children[i].selected = true;
          return;
        }
      }
      throw new Error('Tile not found');
    },

    updateChildren_: function() {
      this.header_.textContent = '';
      this.tileList_.textContent = '';
      if (!this.tiles_)
        return;
      this.header_.textContent = this.tiles_.length + ' inactive tiles';
      this.tileList_.textContent = '';
      this.tiles_.forEach(function(tile) {
        var tileEl = document.createElement('div');
        tileEl.tile = tile;
        tileEl.textContent = tile.id;
        tileEl.className = 'tile';
        this.tileList_.appendChild(tileEl);
      }.bind(this));
    },

    onSelectionChanged_: function() {
      if (this.ignoreChangeEvents_)
        return;
      var tiles = [];
      if (this.tileList_.selectedElement)
        tiles.push(this.tileList_.selectedElement.tile);
      var selection = new InactiveTileViewSelection(this, tiles);
      var e = new base.Event('inactive-tile-selection-changed', true);
      e.selection = selection;
      this.dispatchEvent(e);
    }
  }

  return {
    LTHIView: LTHIView,
    InactiveTileView: InactiveTileView,
  }
});

