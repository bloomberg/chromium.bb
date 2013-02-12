/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.require('tile_view');
base.require('ui');
base.require('ui.list_and_associated_view');

base.requireStylesheet('analysis_view');

base.exportTo('ccfv', function() {
  var TileView = ccfv.TileView;

  var AnalysisView = ui.define('x-analysis-view');

  AnalysisView.prototype = {
    __proto__: HTMLUnknownElement.prototype,

    decorate: function() {
      this.selection_ = undefined;
      this.updateChildren_();
    },

    set selection(selection) {
      if (this.selection_)
        this.selection_.deactivate();
      this.selection_ = selection;
      if (this.selection_)
        this.selection_.activate();
      this.updateChildren_();
    },

    get selection() {
      return this.selection_;
    },

    updateChildren_: function() {
      if (!this.selection_) {
        this.textContent = 'Select something';
        return;
      }
      if (this.selection_.tiles) {
        this.updateChildrenGivenTiles_(this.selection_.tiles);
        return;
      }
      throw new Error('I am confused about what you selected');
    },

    updateChildrenGivenTiles_: function(tiles) {
      this.textContent = '';
      var tileListEl = new ui.ListAndAssociatedView();
      tileListEl.view = new TileView();
      tileListEl.viewProperty = 'tile';
      tileListEl.list = tiles;
      tileListEl.listProperty = 'title';
      this.appendChild(tileListEl);
    },
  };

  return {
    AnalysisView: AnalysisView
  }
});

