// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-query-params.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {selectFolder, setSearchTerm} from './actions.js';
import {BOOKMARKS_BAR_ID} from './constants.js';
import {StoreClient} from './store_client.js';

Polymer({
  /**
   * This element is a one way bound interface that routes the page URL to
   * the searchTerm and selectedId. Clients must initialize themselves by
   * reading the router's fields after attach.
   */
  is: 'bookmarks-router',

  _template: html`{__html_template__}`,

  behaviors: [
    StoreClient,
  ],

  properties: {
    /**
     * Parameter q is routed to the searchTerm.
     * Parameter id is routed to the selectedId.
     * @private
     */
    queryParams_: Object,

    /** @private {string} */
    query_: {
      type: String,
      observer: 'onQueryChanged_',
    },

    /** @private {string} */
    urlQuery_: {
      type: String,
      observer: 'onUrlQueryChanged_',
    },

    /** @private */
    searchTerm_: {
      type: String,
      value: '',
    },

    /** @private {?string} */
    selectedId_: String,
  },

  observers: [
    'onQueryParamsChanged_(queryParams_)',
    'onStateChanged_(searchTerm_, selectedId_)',
  ],

  attached() {
    this.watch('selectedId_', function(state) {
      return state.selectedFolder;
    });
    this.watch('searchTerm_', function(state) {
      return state.search.term;
    });
    this.updateFromStore();
  },

  /** @private */
  onQueryParamsChanged_() {
    const searchTerm = this.queryParams_.q || '';
    let selectedId = this.queryParams_.id;
    if (!selectedId && !searchTerm) {
      selectedId = BOOKMARKS_BAR_ID;
    }

    if (searchTerm !== this.searchTerm_) {
      this.searchTerm_ = searchTerm;
      this.dispatch(setSearchTerm(searchTerm));
    }

    if (selectedId && selectedId !== this.selectedId_) {
      this.selectedId_ = selectedId;
      // Need to dispatch a deferred action so that during page load
      // `this.getState()` will only evaluate after the Store is initialized.
      this.dispatchAsync((dispatch) => {
        dispatch(selectFolder(selectedId, this.getState().nodes));
      });
    }
  },

  /**
   * @param {?string} current Current value of the query.
   * @param {?string} previous Previous value of the query.
   * @private
   */
  onQueryChanged_(current, previous) {
    if (previous !== undefined) {
      this.urlQuery_ = this.query_;
    }
  },

  /** @private */
  onUrlQueryChanged_() {
    this.query_ = this.urlQuery_;
  },

  /** @private */
  onStateChanged_() {
    this.debounce('updateQueryParams', this.updateQueryParams_.bind(this));
  },

  /** @private */
  updateQueryParams_() {
    if (this.searchTerm_) {
      this.queryParams_ = {q: this.searchTerm_};
    } else if (this.selectedId_ !== BOOKMARKS_BAR_ID) {
      this.queryParams_ = {id: this.selectedId_};
    } else {
      this.queryParams_ = {};
    }
  },
});
