// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog walks a user through the flow of setting up Switch
 * Access.
 */

/**
 * Elements that can be hidden or shown for each setup page.
 * The string value should match the element ID in the HTML.
 * @enum {string}
 */
const SASetupElement = {
  BLUETOOTH_BUTTON: 'bluetooth',
  DONE_BUTTON: 'done',
  EXIT_BUTTON: 'exit',
  NEXT_BUTTON: 'next',
  PREVIOUS_BUTTON: 'previous',
  START_OVER_BUTTON: 'start-over',
  INTRO_CONTENT: 'intro',
  ASSIGN_SWITCH_CONTENT: 'assign-switch',
  AUTO_SCAN_ENABLED_CONTENT: 'auto-scan-enabled',
  CHOOSE_SWITCH_COUNT_CONTENT: 'choose-switch-count',
  AUTO_SCAN_SPEED_CONTENT: 'auto-scan-speed',
  CLOSING_CONTENT: 'closing',
};

/**
 * The IDs of each page in the setup flow.
 * @enum {number}
 */
const SASetupPageId = {
  INTRO: 0,
  ASSIGN_SELECT: 1,
  AUTO_SCAN_ENABLED: 2,
  CHOOSE_SWITCH_COUNT: 3,
  AUTO_SCAN_SPEED: 4,
  ASSIGN_NEXT: 5,
  ASSIGN_PREVIOUS: 6,
  TIC_TAC_TOE: 7,
  CLOSING: 8,
};


/**
 * Defines what is visible onscreen for a given page of the setup guide.
 * @typedef {{titleId: string, visibleElements: !Array<SASetupElement>}}
 */
let SASetupPage;

/**
 * A dictionary of all of the dialog pages.
 * @type {Object<SASetupPageId, SASetupPage>}
 */
const SASetupPageList = {};

SASetupPageList[SASetupPageId.INTRO] = {
  titleId: 'switchAccessSetupIntroTitle',
  visibleElements: [
    SASetupElement.BLUETOOTH_BUTTON, SASetupElement.EXIT_BUTTON,
    SASetupElement.NEXT_BUTTON, SASetupElement.INTRO_CONTENT
  ]
};

SASetupPageList[SASetupPageId.ASSIGN_SELECT] = {
  titleId: 'switchAccessSetupAssignSelectTitle',
  visibleElements:
      [SASetupElement.EXIT_BUTTON, SASetupElement.ASSIGN_SWITCH_CONTENT]
};

SASetupPageList[SASetupPageId.AUTO_SCAN_ENABLED] = {
  titleId: 'switchAccessSetupAutoScanEnabledTitle',
  visibleElements: [
    SASetupElement.NEXT_BUTTON, SASetupElement.PREVIOUS_BUTTON,
    SASetupElement.AUTO_SCAN_ENABLED_CONTENT
  ]
};

SASetupPageList[SASetupPageId.CHOOSE_SWITCH_COUNT] = {
  titleId: 'switchAccessSetupChooseSwitchCountTitle',
  visibleElements: [
    SASetupElement.NEXT_BUTTON, SASetupElement.PREVIOUS_BUTTON,
    SASetupElement.CHOOSE_SWITCH_COUNT_CONTENT
  ]
};

SASetupPageList[SASetupPageId.AUTO_SCAN_SPEED] = {
  titleId: 'switchAccessSetupAutoScanSpeedTitle',
  visibleElements: [
    SASetupElement.NEXT_BUTTON, SASetupElement.PREVIOUS_BUTTON,
    SASetupElement.AUTO_SCAN_SPEED_CONTENT
  ]
};

SASetupPageList[SASetupPageId.ASSIGN_NEXT] = {
  titleId: 'switchAccessSetupAssignNextTitle',
  visibleElements:
      [SASetupElement.PREVIOUS_BUTTON, SASetupElement.ASSIGN_SWITCH_CONTENT]
};

SASetupPageList[SASetupPageId.ASSIGN_PREVIOUS] = {
  titleId: 'switchAccessSetupAssignPreviousTitle',
  visibleElements:
      [SASetupElement.PREVIOUS_BUTTON, SASetupElement.ASSIGN_SWITCH_CONTENT]
};

SASetupPageList[SASetupPageId.CLOSING] = {
  titleId: 'switchAccessSetupClosingTitle',
  visibleElements: [
    SASetupElement.DONE_BUTTON, SASetupElement.START_OVER_BUTTON,
    SASetupElement.CLOSING_CONTENT
  ]
};

Polymer({
  is: 'settings-switch-access-setup-guide-dialog',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
  ],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    autoScanPreviouslyEnabled_: {type: Boolean, value: false},

    /** @private {!Array<!cr_slider.SliderTick>} */
    autoScanSpeedRangeMs_: {
      type: Array,
      value: [],
    },

    /** @private */
    currentPageId_: {
      type: Number,
      value: SASetupPageId.INTRO,
    },

    /**
     * A number formatter, to display values with exactly 1 digit after the
     * decimal (that has been internationalized properly).
     * @private {Object}
     */
    formatter_: {
      type: Object,
      value() {
        // navigator.language actually returns a locale, not just a language.
        const locale = window.navigator.language;
        const options = {minimumFractionDigits: 1, maximumFractionDigits: 1};
        return new Intl.NumberFormat(locale, options);
      },
    },

    /** @private */
    maxScanSpeedMs_: {
      readOnly: true,
      type: Number,
      value: AUTO_SCAN_SPEED_RANGE_MS[AUTO_SCAN_SPEED_RANGE_MS.length - 1]
    },

    /** @private */
    maxScanSpeedLabelSec_: {
      readOnly: true,
      type: String,
      value() {
        return this.scanSpeedStringInSec_(this.maxScanSpeedMs_);
      },
    },

    /** @private */
    minScanSpeedMs_:
        {readOnly: true, type: Number, value: AUTO_SCAN_SPEED_RANGE_MS[0]},

    /** @private */
    minScanSpeedLabelSec_: {
      readOnly: true,
      type: String,
      value() {
        return this.scanSpeedStringInSec_(this.minScanSpeedMs_);
      },
    },

    /** @private */
    switchCount_: {
      type: Number,
      value: 1,
    },
  },

  listeners: {
    'exit-pane': 'onSwitchAssignmentMaybeChanged_',
  },

  observers: [
    `onSwitchAssignmentMaybeChanged_(
        prefs.settings.a11y.switch_access.next.*,
        prefs.settings.a11y.switch_access.previous.*,
        prefs.settings.a11y.switch_access.select.*)`,
  ],

  /** @override */
  created() {
    this.autoScanSpeedRangeMs_ =
        this.ticksWithLabelsInSec_(AUTO_SCAN_SPEED_RANGE_MS);
  },

  /** @override */
  attached() {
    SwitchAccessSubpageBrowserProxyImpl.getInstance()
        .notifySwitchAccessSetupGuideAttached();
  },

  /** @override */
  ready() {
    // Reset all switch assignments.
    for (const pref of Object.values(actionToPref)) {
      chrome.settingsPrivate.setPref(pref, {});
    }
  },

  /**
   * @param {SASetupPageId} id
   * @private
   */
  loadPage_(id) {
    this.addOrRemoveAssignmentPane_(id);

    const newPage = SASetupPageList[id];
    this.$.title.textContent = this.i18n(newPage.titleId);

    for (const element of Object.values(SASetupElement)) {
      this['$'][element]['hidden'] = !newPage.visibleElements.includes(element);
    }

    this.currentPageId_ = id;
  },

  /**
   * The assignment pane prevents Switch Access from receiving key events when
   * it is attached, which disables the user's navigational control. Therefore,
   * we add the assignment pane only when it's about to be displayed, and remove
   * it as soon as it's complete.
   * @param {SASetupPageId} id
   * @private
   */
  addOrRemoveAssignmentPane_(id) {
    let action;
    switch (id) {
      case SASetupPageId.ASSIGN_SELECT:
        action = SwitchAccessCommand.SELECT;
        break;
      case SASetupPageId.ASSIGN_NEXT:
        action = SwitchAccessCommand.NEXT;
        break;
      case SASetupPageId.ASSIGN_PREVIOUS:
        action = SwitchAccessCommand.PREVIOUS;
    }

    if (action) {
      this.initializeAssignmentPane_(action);
    } else {
      this.removeAssignmentPaneIfPresent_();
    }
  },

  /**
   * @param {SwitchAccessCommand} action
   * @private
   */
  initializeAssignmentPane_(action) {
    this.removeAssignmentPaneIfPresent_();

    this.assignmentIllustrationElement.classList.add(action);

    const assignmentPane =
        document.createElement('settings-switch-access-action-assignment-pane');
    assignmentPane.action = action;
    assignmentPane.context = AssignmentContext.SETUP_GUIDE;
    this.assignmentContentsElement.appendChild(assignmentPane);
  },

  /** @private */
  removeAssignmentPaneIfPresent_() {
    if (this.assignmentContentsElement.firstChild) {
      this.assignmentContentsElement.removeChild(
          this.assignmentContentsElement.firstChild);
    }
    this.assignmentIllustrationElement.classList = 'illustration';
  },

  /**
   * Determines what page is shown next, from the current page ID and other
   * state.
   * @return {!SASetupPageId}
   * @private
   */
  getNextPageId_() {
    switch (this.currentPageId_) {
      case SASetupPageId.INTRO:
        return SASetupPageId.ASSIGN_SELECT;
      case SASetupPageId.ASSIGN_SELECT:
        return SASetupPageId.AUTO_SCAN_ENABLED;
      case SASetupPageId.AUTO_SCAN_ENABLED:
        return SASetupPageId.CHOOSE_SWITCH_COUNT;
      case SASetupPageId.CHOOSE_SWITCH_COUNT:
        if (this.switchCount_ === 3 || this.switchCount_ === 2) {
          return SASetupPageId.ASSIGN_NEXT;
        } else {
          return SASetupPageId.AUTO_SCAN_SPEED;
        }
      case SASetupPageId.ASSIGN_NEXT:
        if (this.switchCount_ === 3) {
          return SASetupPageId.ASSIGN_PREVIOUS;
        } else {
          return SASetupPageId.CLOSING;
        }
      case SASetupPageId.ASSIGN_PREVIOUS:
      case SASetupPageId.AUTO_SCAN_SPEED:
      default:
        return SASetupPageId.CLOSING;
    }
  },

  /**
   * Returns what page was shown previously from the current page ID.
   * @return {!SASetupPageId}
   * @private
   */
  getPreviousPageId_() {
    switch (this.currentPageId_) {
      case SASetupPageId.CLOSING:
        if (this.switchCount_ === 3) {
          return SASetupPageId.ASSIGN_PREVIOUS;
        } else if (this.switchCount_ === 2) {
          return SASetupPageId.ASSIGN_NEXT;
        } else {
          return SASetupPageId.AUTO_SCAN_SPEED;
        }
      case SASetupPageId.ASSIGN_PREVIOUS:
        return SASetupPageId.ASSIGN_NEXT;
      case SASetupPageId.ASSIGN_NEXT:
      case SASetupPageId.AUTO_SCAN_SPEED:
        return SASetupPageId.CHOOSE_SWITCH_COUNT;
      case SASetupPageId.CHOOSE_SWITCH_COUNT:
        return SASetupPageId.AUTO_SCAN_ENABLED;
      case SASetupPageId.AUTO_SCAN_ENABLED:
        return SASetupPageId.ASSIGN_SELECT;
      case SASetupPageId.ASSIGN_SELECT:
      default:
        return SASetupPageId.INTRO;
    }
  },

  /** @private */
  onExitClick_() {
    this.$.switchAccessSetupGuideDialog.close();
  },

  /** @private */
  onStartOverClick_() {
    this.loadPage_(SASetupPageId.INTRO);
  },

  /** @private */
  onNextClick_() {
    this.loadPage_(this.getNextPageId_());

    // Enable auto-scan when we reach that page of the setup guide.
    if (this.currentPageId_ === SASetupPageId.AUTO_SCAN_ENABLED) {
      this.autoScanPreviouslyEnabled_ = /** @type {boolean} */ (
          this.getPref('settings.a11y.switch_access.auto_scan.enabled').value);
      chrome.settingsPrivate.setPref(
          'settings.a11y.switch_access.auto_scan.enabled', true);
    }

    if (this.currentPageId_ === SASetupPageId.ASSIGN_NEXT) {
      chrome.settingsPrivate.setPref(
          'settings.a11y.switch_access.auto_scan.enabled', false);
    }

    if (this.currentPageId_ === SASetupPageId.CLOSING) {
      if (this.switchCount_ >= 2) {
        this['$']['closing-instructions'].textContent =
            this.i18n('switchAccessSetupClosingManualScanInstructions');
      } else {
        this['$']['closing-instructions'].textContent =
            this.i18n('switchAccessSetupClosingAutoScanInstructions');
      }
    }
  },

  /** @private */
  onPreviousClick_() {
    // Disable auto-scan, if we enabled it as part of the setup guide.
    if (this.currentPageId_ === SASetupPageId.AUTO_SCAN_ENABLED &&
        !this.autoScanPreviouslyEnabled_) {
      chrome.settingsPrivate.setPref(
          'settings.a11y.switch_access.auto_scan.enabled', false);
    }

    this.loadPage_(this.getPreviousPageId_());
  },

  /** @private */
  onBluetoothClick_() {
    settings.Router.getInstance().navigateTo(settings.routes.BLUETOOTH_DEVICES);
  },

  /** @private */
  onAutoScanSpeedFaster_() {
    const currentValue = /** @type {number} */ (
        this.getPref('settings.a11y.switch_access.auto_scan.speed_ms').value);
    const index =
        AUTO_SCAN_SPEED_RANGE_MS.findIndex(elem => elem <= currentValue);
    if (index === -1 || index === AUTO_SCAN_SPEED_RANGE_MS.length - 1) {
      return;
    }
    chrome.settingsPrivate.setPref(
        'settings.a11y.switch_access.auto_scan.speed_ms',
        AUTO_SCAN_SPEED_RANGE_MS[index + 1]);
  },

  /** @private */
  onAutoScanSpeedSlower_() {
    const currentValue = /** @type {number} */ (
        this.getPref('settings.a11y.switch_access.auto_scan.speed_ms').value);
    const index =
        AUTO_SCAN_SPEED_RANGE_MS.findIndex(elem => elem <= currentValue);
    if (index <= 0) {
      return;
    }
    chrome.settingsPrivate.setPref(
        'settings.a11y.switch_access.auto_scan.speed_ms',
        AUTO_SCAN_SPEED_RANGE_MS[index - 1]);
  },

  /** @private */
  onSwitchCountChanged_() {
    const selected = this['$']['switch-count-group'].selected;
    if (selected === 'one-switch') {
      this.switchCount_ = 1;
      this['$']['choose-switch-count-illustration'].className =
          'illustration one-switch';
    } else if (selected === 'two-switches') {
      this.switchCount_ = 2;
      this['$']['choose-switch-count-illustration'].className =
          'illustration two-switches';
    } else if (selected === 'three-switches') {
      this.switchCount_ = 3;
      this['$']['choose-switch-count-illustration'].className =
          'illustration three-switches';
    }
  },

  /** @private */
  onSwitchAssignmentMaybeChanged_() {
    if (!this.assignmentContentsElement ||
        !this.assignmentContentsElement.firstChild) {
      return;
    }

    const currentAction = this.assignmentContentsElement.firstChild.action;
    const prefValue = /** @type {!Object} */ (
        this.getPref(actionToPref[currentAction]).value);
    const hasSwitchAssigned = Object.keys(prefValue).length > 0;

    if (hasSwitchAssigned) {
      this.onNextClick_();
    } else {
      this.initializeAssignmentPane_(currentAction);
    }
  },

  /**
   * @param {number} scanSpeedValueMs
   * @return {string} a string representing the scan speed in seconds.
   * @private
   */
  scanSpeedStringInSec_(scanSpeedValueMs) {
    const scanSpeedValueSec = scanSpeedValueMs / 1000;
    return this.i18n(
        'durationInSeconds', this.formatter_.format(scanSpeedValueSec));
  },

  /**
   * @param {!Array<number>} ticksInMs
   * @return {!Array<!cr_slider.SliderTick>}
   * @private
   */
  ticksWithLabelsInSec_(ticksInMs) {
    // Dividing by 1000 to convert milliseconds to seconds for the label.
    return ticksInMs.map(
        x => ({label: `${this.scanSpeedStringInSec_(x)}`, value: x}));
  },

  /** @private */
  get assignmentContentsElement() {
    return this['$'][SASetupElement.ASSIGN_SWITCH_CONTENT].querySelector(
        '.sa-setup-contents');
  },

  /** @private */
  get assignmentIllustrationElement() {
    return this['$'][SASetupElement.ASSIGN_SWITCH_CONTENT].querySelector(
        '.illustration');
  },
});
