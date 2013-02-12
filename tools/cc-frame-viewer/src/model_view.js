/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.require('ui');
base.require('model');
base.require('ui.drag_handle');
base.require('ui.list_view');
base.require('analysis_view');
base.require('lthi_view');
base.requireStylesheet('model_view');

base.exportTo('ccfv', function() {
  var ModelView = ui.define('x-model-view');

  ModelView.prototype = {
    __proto__: HTMLUnknownElement.prototype,

    decorate: function() {
      this.model_ = undefined;

      this.lthiHistorySelector_ = document.createElement('select');
      this.lthiHistorySelector_.addEventListener(
        'change', this.onLTHISelectorChanged_);

      this.lthiView_ = new ccfv.LTHIView();
      this.lthiView_.addEventListener(
        'selection-changed',
        this.onLTHIViewSelectionChanged_.bind(this));

      this.lthiList_ = new ui.ListView();
      this.lthiList_.classList.add('lthi-list');
      this.lthiList_.addEventListener(
        'selection-changed',
        this.updateLTHIView_.bind(this));

      this.analysisView_ = new ccfv.AnalysisView();

      var lthiPicker = document.createElement('div');
      lthiPicker.className = 'lthi-picker';
      lthiPicker.appendChild(this.lthiHistorySelector_);

      var lthiAndAnalysisView = document.createElement('div');
      lthiAndAnalysisView.className = 'lthi-and-analysis-view';
      lthiAndAnalysisView.appendChild(this.lthiView_);
      var dragHandle = new ui.DragHandle();
      dragHandle.target = this.analysisView_;
      lthiAndAnalysisView.appendChild(dragHandle);
      lthiAndAnalysisView.appendChild(this.analysisView_);

      var lthiView = document.createElement('div');
      lthiView.className = 'lthi-view';
      lthiView.appendChild(this.lthiList_);
      lthiView.appendChild(lthiAndAnalysisView);

      this.appendChild(lthiPicker);
      this.appendChild(lthiView);
    },

    get model() {
      return this.model_;
    },

    set model(model) {
      this.model_ = model;
      this.updateLTHISelector_();
    },

    updateLTHISelector_: function() {
      this.lthiHistorySelector_.textContent = '';
      this.lthiList_.textContent = '';
      if (!this.model_)
        return this.updateLTHIList_();

      var lthiHistories = base.dictionaryValues(this.model_.lthiHistories);
      lthiHistories.forEach(function(lthiHistory) {
        var option = document.createElement('option');
        option.textContent = 'LTHI ' + lthiHistory.id;
        option.lthiHistory = lthiHistory;
        this.lthiHistorySelector_.appendChild(option);
      }.bind(this));
      this.updateLTHIList_();
    },

    onLTHISelectorChanged_: function() {
      this.updateLTHIList_();
    },

    get selectedLTHI() {
      var el = this.lthiList_.selectedElement;
      if (!el)
        return undefined;
      return el.lthi;
    },

    updateLTHIList_: function() {
      var lthiHistory =
        this.lthiHistorySelector_.selectedOptions[0].lthiHistory;
      this.lthiList_.textContent = '';
      lthiHistory.lthiSnapshots.forEach(function(lthi, index) {
        var lthiItem = document.createElement('div');
        lthiItem.lthi = lthi;
        this.lthiList_.appendChild(lthiItem);

        lthiItem.selected = index == 0;
        lthiItem.textContent = 'Frame ' + lthi.snapshotNumber;
      }.bind(this));
      this.updateLTHIView_();
    },

    updateLTHIView_: function() {
      if (this.lthiView_.lthi != this.selectedLTHI)
        this.lthiView_.lthi = this.selectedLTHI;
    },

    onLTHIViewSelectionChanged_: function(e) {
      this.analysisView_.selection = e.selection;
    }
  };

  return {
    ModelView: ModelView,
  }
});

