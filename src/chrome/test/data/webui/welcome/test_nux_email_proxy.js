// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/** @implements {nux.ModuleMetricsProxy} */
class TestEmailMetricsProxy extends TestBrowserProxy {
  constructor() {
    super([
      'recordChoseAnOptionAndChoseNext',
      'recordChoseAnOptionAndChoseSkip',
      'recordChoseAnOptionAndNavigatedAway',
      'recordClickedDisabledNextButtonAndChoseNext',
      'recordClickedDisabledNextButtonAndChoseSkip',
      'recordClickedDisabledNextButtonAndNavigatedAway',
      'recordDidNothingAndChoseNext',
      'recordDidNothingAndChoseSkip',
      'recordDidNothingAndNavigatedAway',
      'recordNavigatedAwayThroughBrowserHistory',
      'recordPageShown',
    ]);
  }

  /** @override */
  recordChoseAnOptionAndChoseNext() {
    this.methodCalled('recordChoseAnOptionAndChoseNext');
  }

  /** @override */
  recordChoseAnOptionAndChoseSkip() {
    this.methodCalled('recordChoseAnOptionAndChoseSkip');
  }

  /** @override */
  recordChoseAnOptionAndNavigatedAway() {
    this.methodCalled('recordChoseAnOptionAndNavigatedAway');
  }

  /** @override */
  recordClickedDisabledNextButtonAndChoseNext() {
    this.methodCalled('recordClickedDisabledNextButtonAndChoseNext');
  }

  /** @override */
  recordClickedDisabledNextButtonAndChoseSkip() {
    this.methodCalled('recordClickedDisabledNextButtonAndChoseSkip');
  }

  /** @override */
  recordClickedDisabledNextButtonAndNavigatedAway() {
    this.methodCalled('recordClickedDisabledNextButtonAndNavigatedAway');
  }

  /** @override */
  recordDidNothingAndChoseNext() {
    this.methodCalled('recordDidNothingAndChoseNext');
  }

  /** @override */
  recordDidNothingAndChoseSkip() {
    this.methodCalled('recordDidNothingAndChoseSkip');
  }

  /** @override */
  recordDidNothingAndNavigatedAway() {
    this.methodCalled('recordDidNothingAndNavigatedAway');
  }

  /** @override */
  recordNavigatedAwayThroughBrowserHistory() {
    this.methodCalled('recordNavigatedAwayThroughBrowserHistory');
  }

  /** @override */
  recordPageShown() {
    this.methodCalled('recordPageShown');
  }
}

/** @implements {nux.AppProxy} */
class TestNuxEmailProxy extends TestBrowserProxy {
  constructor() {
    super([
      'cacheBookmarkIcon',
      'getAppList',
      'getSavedProvider',
      'recordProviderSelected',
    ]);

    /** @private {!Array<!nux.BookmarkListItem>} */
    this.emailList_ = [];

    /** @private {number} */
    this.stubSavedProvider_;
  }

  /** @param {number} id */
  setSavedProvider(id) {
    this.stubSavedProvider_ = id;
  }

  /** @override */
  getAppList() {
    this.methodCalled('getAppList');
    return Promise.resolve(this.emailList_);
  }

  /** @override */
  cacheBookmarkIcon() {
    this.methodCalled('cacheBookmarkIcon');
  }

  getSavedProvider() {
    this.methodCalled('getSavedProvider');
    return this.stubSavedProvider_;
  }

  /** @override */
  recordProviderSelected() {
    this.methodCalled('recordProviderSelected', arguments);
  }

  /** @param {!Array<!nux.BookmarkListItem>} emailList */
  setEmailList(emailList) {
    this.emailList_ = emailList;
  }
}
