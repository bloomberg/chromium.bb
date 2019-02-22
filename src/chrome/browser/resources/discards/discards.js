// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('discards', function() {
  'use strict';

  // The following variables are initialized by 'initialize'.
  // Points to the Mojo WebUI handler.
  let uiHandler;

  /**
   * @return {!mojom.DiscardsDetailsProviderPtr} The UI handler.
   */
  function getOrCreateUiHandler() {
    if (!uiHandler) {
      uiHandler = new mojom.DiscardsDetailsProviderPtr;
      Mojo.bindInterface(
          mojom.DiscardsDetailsProvider.name,
          mojo.makeRequest(uiHandler).handle);
    }
    return uiHandler;
  }

  /**
   * @param {mojom.LifecycleUnitState} state The discard state.
   * @return {boolean} Whether the state is related to discarding.
   */
  function isDiscardRelatedState(state) {
    return state == mojom.LifecycleUnitState.PENDING_DISCARD ||
        state == mojom.LifecycleUnitState.DISCARDED;
  }

  /**
   * Compares two TabDiscardsInfos based on the data in the provided sort-key.
   * @param {string} sortKey The key of the sort. See the "data-sort-key"
   *     attribute of the table headers for valid sort-keys.
   * @param {boolean|number|string} a The first value being compared.
   * @param {boolean|number|string} b The second value being compared.
   * @return {number} A negative number if a < b, 0 if a == b, and a positive
   *     number if a > b.
   */
  function compareTabDiscardsInfos(sortKey, a, b) {
    let val1 = a[sortKey];
    let val2 = b[sortKey];

    // Compares strings.
    if (sortKey == 'title' || sortKey == 'tabUrl') {
      val1 = val1.toLowerCase();
      val2 = val2.toLowerCase();
      if (val1 == val2)
        return 0;
      return val1 > val2 ? 1 : -1;
    }

    // Compares boolean fields.
    if (['canFreeze', 'canDiscard', 'isAutoDiscardable'].includes(sortKey)) {
      if (val1 == val2)
        return 0;
      return val1 ? 1 : -1;
    }

    // Compare lifecycle state. This is actually a compound key.
    if (sortKey == 'state') {
      // If the keys are discarding state, then break ties using the discard
      // reason.
      if (val1 == val2 && isDiscardRelatedState(val1)) {
        val1 = a['discardReason'];
        val2 = b['discardReason'];
      }
      return val1 - val2;
    }

    // Compares numeric fields.
    // NOTE: visibility, loadingState and state are represented as a numeric
    // value.
    if ([
          'visibility',
          'loadingState',
          'discardCount',
          'utilityRank',
          'reactivationScore',
          'lastActiveSeconds',
          'siteEngagementScore',
        ].includes(sortKey)) {
      return val1 - val2;
    }

    assertNotReached('Unsupported sort key: ' + sortKey);
    return 0;
  }

  /**
   * Pluralizes a string according to the given count. Assumes that appending an
   * 's' is sufficient to make a string plural.
   * @param {string} s The string to be made plural if necessary.
   * @param {number} n The count of the number of ojects.
   * @return {string} The plural version of |s| if n != 1, otherwise |s|.
   */
  function maybeMakePlural(s, n) {
    return n == 1 ? s : s + 's';
  }

  /**
   * Converts a |seconds| interval to a user friendly string.
   * @param {number} seconds The interval to render.
   * @return {string} An English string representing the interval.
   */
  function secondsToString(seconds) {
    // These constants aren't perfect, but close enough.
    const SECONDS_PER_MINUTE = 60;
    const MINUTES_PER_HOUR = 60;
    const SECONDS_PER_HOUR = SECONDS_PER_MINUTE * MINUTES_PER_HOUR;
    const HOURS_PER_DAY = 24;
    const SECONDS_PER_DAY = SECONDS_PER_HOUR * HOURS_PER_DAY;
    const DAYS_PER_WEEK = 7;
    const SECONDS_PER_WEEK = SECONDS_PER_DAY * DAYS_PER_WEEK;
    const SECONDS_PER_MONTH = SECONDS_PER_DAY * 30.5;
    const SECONDS_PER_YEAR = SECONDS_PER_DAY * 365;

    // Seconds.
    if (seconds < SECONDS_PER_MINUTE)
      return seconds.toString() + maybeMakePlural(' second', seconds);

    // Minutes.
    let minutes = Math.floor(seconds / SECONDS_PER_MINUTE);
    if (minutes < MINUTES_PER_HOUR) {
      return minutes.toString() + maybeMakePlural(' minute', minutes);
    }

    // Hours and minutes.
    let hours = Math.floor(seconds / SECONDS_PER_HOUR);
    minutes = minutes % MINUTES_PER_HOUR;
    if (hours < HOURS_PER_DAY) {
      let s = hours.toString() + maybeMakePlural(' hour', hours);
      if (minutes > 0) {
        s += ' and ' + minutes.toString() + maybeMakePlural(' minute', minutes);
      }
      return s;
    }

    // Days.
    let days = Math.floor(seconds / SECONDS_PER_DAY);
    if (days < DAYS_PER_WEEK) {
      return days.toString() + maybeMakePlural(' day', days);
    }

    // Weeks. There's an awkward gap to bridge where 4 weeks can have
    // elapsed but not quite 1 month. Be sure to use weeks to report that.
    let weeks = Math.floor(seconds / SECONDS_PER_WEEK);
    let months = Math.floor(seconds / SECONDS_PER_MONTH);
    if (months < 1) {
      return 'over ' + weeks.toString() + maybeMakePlural(' week', weeks);
    }

    // Months.
    let years = Math.floor(seconds / SECONDS_PER_YEAR);
    if (years < 1) {
      return 'over ' + months.toString() + maybeMakePlural(' month', months);
    }

    // Years.
    return 'over ' + years.toString() + maybeMakePlural(' year', years);
  }

  /**
   * Converts a |secondsAgo| duration to a user friendly string.
   * @param {number} secondsAgo The duration to render.
   * @return {string} An English string representing the duration.
   */
  function durationToString(secondsAgo) {
    let ret = secondsToString(secondsAgo);

    if (ret.endsWith(' seconds') || ret.endsWith(' second'))
      return 'just now';

    return ret + ' ago';
  }

  /**
   * Returns a string representation of a boolean value for display in a table.
   * @param {boolean} bool A boolean value.
   * @return {string} A string representing the bool.
   */
  function boolToString(bool) {
    return bool ? '✔' : '✘️';
  }

  // These functions are exposed on the 'discards' object created by
  // cr.define. This allows unittesting of these functions.
  return {
    boolToString: boolToString,
    compareTabDiscardsInfos: compareTabDiscardsInfos,
    durationToString: durationToString,
    getOrCreateUiHandler: getOrCreateUiHandler,
    maybeMakePlural: maybeMakePlural,
    secondsToString: secondsToString
  };
});
