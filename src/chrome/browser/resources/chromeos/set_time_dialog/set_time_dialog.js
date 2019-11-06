// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'set-time-dialog' handles a dialog to check and set system time. It can also
 * include a timezone dropdown if timezoneId is provided.
 *
 * 'set-time-dialog' uses the system time to populate the controls initially and
 * update them as the system time or timezone changes, and notifies Chrome
 * when the user changes the time or timezone.
 */

(function() {
'use strict';

/**
 * @return {!Array<!{id: string, name: string, selected: Boolean}>} Items for
 *     the timezone select element.
 */
function getTimezoneItems() {
  const currentTimezoneId =
      /** @type {string} */ (loadTimeData.getValue('currentTimezoneId'));
  const timezoneList =
      /** @type {!Array} */ (loadTimeData.getValue('timezoneList'));
  return timezoneList.map(
      tz => ({id: tz[0], name: tz[1], selected: tz[0] === currentTimezoneId}));
}

/**
 * Builds date and time strings suitable for the values of HTML date and
 * time elements.
 * @param {!Date} date The date object to represent.
 * @return {!{date: string, time: string}} Date is an RFC 3339 formatted date
 *     and time is an HH:MM formatted time.
 * @private
 */
function dateToHtmlValues(date) {
  // Get the current time and subtract the timezone offset, so the
  // JSON string is in local time.
  const localDate = new Date(date);
  localDate.setMinutes(date.getMinutes() - date.getTimezoneOffset());
  return {
    date: localDate.toISOString().slice(0, 10),
    time: localDate.toISOString().slice(11, 16)
  };
}

/**
 * @return {string} Minimum date for the date picker in RFC 3339 format.
 */
function getMinDate() {
  // Start with the build date because we can't trust the clock. The build time
  // doesn't include a timezone, so subtract 1 day to get a safe minimum date.
  let minDate = new Date(loadTimeData.getValue('buildTime'));
  minDate.setDate(minDate.getDate() - 1);
  // Make sure the ostensible date is in range.
  const now = new Date();
  if (now < minDate)
    minDate = now;
  // Convert to string for date input min attribute.
  return dateToHtmlValues(minDate).date;
}

/**
 * @return {string} Maximum date for the date picker in RFC 3339 format.
 */
function getMaxDate() {
  // Set the max date to the build date plus 20 years.
  let maxDate = new Date(loadTimeData.getValue('buildTime'));
  maxDate.setFullYear(maxDate.getFullYear() + 20);
  // Make sure the ostensible date is in range.
  const now = new Date();
  if (now > maxDate)
    maxDate = now;
  // Convert to string for date input max attribute.
  return dateToHtmlValues(maxDate).date;
}

Polymer({
  is: 'set-time-dialog',

  // Remove listeners on detach.
  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * Items to populate the timezone select.
     * @private
     */
    timezoneItems_: {
      type: Array,
      readonly: true,
      value: getTimezoneItems,
    },

    /**
     * Whether the timezone select element is visible.
     * @private
     */
    isTimezoneVisible_: {
      type: Boolean,
      readonly: true,
      value: () => loadTimeData.getValue('currentTimezoneId') != '',
    },

    /**
     * The minimum date allowed in the date picker.
     * @private
     */
    minDate_: {
      type: String,
      readonly: true,
      value: getMinDate,
    },

    /**
     * The maximum date allowed in the date picker.
     * @private
     */
    maxDate_: {
      type: String,
      readonly: true,
      value: getMaxDate,
    },
  },

  /**
   * Values for reverting inputs when the user's date/time is invalid.
   * @private {?Object}
   */
  prevValues_: null,

  /**
   * ID of the setTimeout() used to refresh the current time.
   * @private {?number}
   */
  timeTimeoutId_: null,

  /** @private {?settime.SetTimeBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.prevValues_ = {};
    this.browserProxy_ = settime.SetTimeBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    this.updateTime_();
  },

  /** @override */
  attached: function() {
    // Register listeners for updates from C++ code.
    this.addWebUIListener('system-clock-updated', this.updateTime_.bind(this));
    this.addWebUIListener(
        'system-timezone-changed', this.setTimezone_.bind(this));

    this.browserProxy_.sendPageReady();

    /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
  },

  /**
   * Sets the current timezone.
   * @param {string} timezoneId The timezone ID to select.
   * @private
   */
  setTimezone_: function(timezoneId) {
    const timezoneSelect = this.$$('#timezoneSelect');
    assert(timezoneSelect.childElementCount > 0);
    timezoneSelect.value = timezoneId;
    this.updateTime_();
  },

  /**
   * Updates the date/time controls to the current local time.
   * Called initially, then called again once a minute.
   * @private
   */
  updateTime_: function() {
    const now = new Date();

    // Only update time controls if neither is focused.
    if (document.activeElement.id != 'dateInput' &&
        document.activeElement.id != 'timeInput') {
      const htmlValues = dateToHtmlValues(now);
      this.prevValues_.date = this.$.dateInput.value = htmlValues.date;
      this.prevValues_.time = this.$.timeInput.value = htmlValues.time;
    }

    if (this.timeTimeoutId_) {
      window.clearTimeout(this.timeTimeoutId_);
    }

    // Start timer to update these inputs every minute.
    const secondsRemaining = 60 - now.getSeconds();
    this.timeTimeoutId_ =
        window.setTimeout(this.updateTime_.bind(this), secondsRemaining * 1000);
  },

  /**
   * Sets the system time from the UI.
   * @private
   */
  applyTime_: function() {
    const date = this.$.dateInput.valueAsDate;
    date.setMilliseconds(
        date.getMilliseconds() + this.$.timeInput.valueAsNumber);

    // Add timezone offset to get real time.
    date.setMinutes(date.getMinutes() + date.getTimezoneOffset());

    const seconds = Math.floor(date / 1000);
    this.browserProxy_.setTimeInSeconds(seconds);
  },

  /**
   * Called when focus is lost on date/time controls.
   * @param {!Event} e The blur event.
   * @private
   */
  onTimeBlur_: function(e) {
    if (e.target.validity.valid && e.target.value) {
      // Make this the new fallback time in case of future invalid input.
      this.prevValues_[e.target.id] = e.target.value;
      this.applyTime_();
    } else {
      // Restore previous value.
      e.target.value = this.prevValues_[e.target.id];
    }
  },

  /**
   * @param {!Event} e The change event.
   * @private
   */
  onTimezoneChange_: function(e) {
    this.browserProxy_.setTimezone(e.currentTarget.value);
  },

  /** @private */
  onDoneClick_: function() {
    this.browserProxy_.dialogClose();
  },
});
})();
