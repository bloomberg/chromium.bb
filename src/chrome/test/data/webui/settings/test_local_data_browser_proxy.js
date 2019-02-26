// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A test version of LocalDataBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 *
 * @implements {settings.LocalDataBrowserProxy}
 */
class TestLocalDataBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getDisplayList',
      'removeAll',
      'removeShownItems',
      'removeItem',
      'getCookieDetails',
      'getNumCookiesList',
      'getNumCookiesString',
      'reloadCookies',
      'removeCookie',
    ]);

    /** @private {?CookieList} */
    this.cookieDetails_ = null;

    /** @private {Array<!CookieList>} */
    this.cookieList_ = [];
  }

  /**
   * Test-only helper.
   * @param {!CookieList} cookieDetails
   */
  setCookieDetails(cookieDetails) {
    this.cookieDetails_ = cookieDetails;
  }

  /**
   * Test-only helper.
   * @param {!CookieList} cookieList
   */
  setCookieList(cookieList) {
    this.cookieList_ = cookieList;
    this.filteredCookieList_ = cookieList;
  }

  /** @override */
  getDisplayList(filter) {
    if (filter === undefined)
      filter = '';
    let output = [];
    for (let i = 0; i < this.cookieList_.length; ++i) {
      if (this.cookieList_[i].site.indexOf(filter) >= 0) {
        output.push(this.filteredCookieList_[i]);
      }
    }
    return Promise.resolve({items: output});
  }

  /** @override */
  removeAll() {
    this.methodCalled('removeAll');
    return Promise.resolve({id: null, children: []});
  }

  /** @override */
  removeShownItems() {
    this.methodCalled('removeShownItems');
  }

  /** @override */
  removeItem(id) {
    this.methodCalled('removeItem', id);
  }

  /** @override */
  getCookieDetails(site) {
    this.methodCalled('getCookieDetails', site);
    return Promise.resolve(this.cookieDetails_ || {id: '', children: []});
  }

  /** @override */
  getNumCookiesList(siteList) {
    this.methodCalled('getNumCookiesList', siteList);
    const numCookiesMap = new Map();
    if (this.cookieDetails_) {
      this.cookieDetails_.children.forEach(cookie => {
        let numCookies = numCookiesMap.get(cookie.domain);
        numCookies = numCookies == null ? 1 : ++numCookies;
        numCookiesMap.set(cookie.domain, numCookies);
      });
    }

    const numCookiesList = siteList.map(site => {
      const numCookies = numCookiesMap.get(site);
      return {
        etldPlus1: site,
        numCookies: numCookies == null ? 0 : numCookies,
      };
    });
    return Promise.resolve(numCookiesList);
  }

  /** @override */
  getNumCookiesString(numCookies) {
    this.methodCalled('getNumCookiesString', numCookies);
    return Promise.resolve(
        `${numCookies} ` + (numCookies == 1 ? 'cookie' : 'cookies'));
  }

  /** @override */
  reloadCookies() {
    this.methodCalled('reloadCookies');
    return Promise.resolve({id: null, children: []});
  }

  /** @override */
  removeCookie(path) {
    this.methodCalled('removeCookie', path);
  }
}
