// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/** @enum {number} */
print_preview.DestinationState = {
  INIT: 0,
  SELECTED: 1,
  SET: 2,
  UPDATED: 3,
  ERROR: 4,
};

(function() {
'use strict';

/** @type {number} Number of recent destinations to save. */
const NUM_PERSISTED_DESTINATIONS = 3;

Polymer({
  is: 'print-preview-destination-settings',

  behaviors: [
    I18nBehavior,
    SettingsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    appKioskMode: Boolean,

    /** @type {cloudprint.CloudPrintInterface} */
    cloudPrintInterface: {
      type: Object,
      observer: 'onCloudPrintInterfaceSet_',
    },

    dark: Boolean,

    /** @type {?print_preview.Destination} */
    destination: {
      type: Object,
      notify: true,
      value: null,
    },

    /** @private {!print_preview.DestinationState} */
    destinationState: {
      type: Number,
      notify: true,
      value: print_preview.DestinationState.INIT,
      observer: 'updateDestinationSelect_',
    },

    disabled: Boolean,

    /** @type {!print_preview_new.Error} */
    error: {
      type: Number,
      notify: true,
    },

    /** @type {!print_preview_new.State} */
    state: Number,

    /** @private {string} */
    activeUser_: {
      type: String,
      observer: 'onActiveUserChanged_',
    },

    /** @private {!print_preview.CloudPrintState} */
    cloudPrintState_: {
      type: Number,
      value: print_preview.CloudPrintState.DISABLED,
    },

    /** @private {?print_preview.DestinationStore} */
    destinationStore_: {
      type: Object,
      value: null,
    },

    // <if expr="chromeos">
    hasPinSetting_: {
      type: Boolean,
      computed: 'computeHasPinSetting_(settings.pin.available)',
      reflectToAttribute: true,
    },
    // </if>

    /** @private {?print_preview.InvitationStore} */
    invitationStore_: {
      type: Object,
      value: null,
    },

    /** @private {boolean} */
    noDestinations_: {
      type: Boolean,
      value: false,
    },

    /** @private {!Array<!print_preview.Destination>} */
    recentDestinationList_: Array,

    /** @private */
    shouldHideSpinner_: {
      type: Boolean,
      computed: 'computeShouldHideSpinner_(destinationState, destination)',
    },

    /** @private {string} */
    statusText_: {
      type: String,
      computed: 'computeStatusText_(destination)',
    },

    /** @private {!Array<string>} */
    users_: Array,
  },

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @override */
  attached: function() {
    this.destinationStore_ =
        new print_preview.DestinationStore(this.addWebUIListener.bind(this));
    this.invitationStore_ = new print_preview.InvitationStore();
    this.tracker_.add(
        this.destinationStore_,
        print_preview.DestinationStore.EventType.DESTINATION_SELECT,
        this.onDestinationSelect_.bind(this));
    this.tracker_.add(
        this.destinationStore_,
        print_preview.DestinationStore.EventType
            .SELECTED_DESTINATION_CAPABILITIES_READY,
        this.onDestinationCapabilitiesReady_.bind(this));
    this.tracker_.add(
        this.destinationStore_, print_preview.DestinationStore.EventType.ERROR,
        this.onDestinationError_.bind(this));
    // Need to update the recent list when the destination store inserts
    // destinations, in case any recent destinations have been added to the
    // store. At startup, recent destinations can be in the sticky settings,
    // but they should not be displayed in the dropdown until they have been
    // fetched by the DestinationStore, to ensure that they still exist.
    this.tracker_.add(
        assert(this.destinationStore_),
        print_preview.DestinationStore.EventType.DESTINATIONS_INSERTED,
        this.updateDropdownDestinations_.bind(this));
  },

  /** @override */
  detached: function() {
    this.invitationStore_.resetTracker();
    this.destinationStore_.resetTracker();
    this.tracker_.removeAll();
  },

  /** @private */
  onCloudPrintInterfaceSet_: function() {
    const cloudPrintInterface = assert(this.cloudPrintInterface);
    [cloudprint.CloudPrintInterfaceEventType.SEARCH_FAILED,
     cloudprint.CloudPrintInterfaceEventType.PRINTER_FAILED,
    ].forEach(eventType => {
      this.tracker_.add(
          cloudPrintInterface.getEventTarget(), eventType,
          this.checkCloudPrintStatus_.bind(this));
    });
    this.$.userInfo.setCloudPrintInterface(cloudPrintInterface);
    this.destinationStore_.setCloudPrintInterface(cloudPrintInterface);
    this.invitationStore_.setCloudPrintInterface(cloudPrintInterface);
    assert(this.cloudPrintState_ === print_preview.CloudPrintState.DISABLED);
    this.cloudPrintState_ = print_preview.CloudPrintState.ENABLED;
  },

  /**
   * @param {string} defaultPrinter The system default printer ID.
   * @param {string} serializedDefaultDestinationRulesStr String with rules for
   *     selecting a default destination.
   */
  initDestinationStore: function(
      defaultPrinter, serializedDefaultDestinationRulesStr) {
    this.destinationStore_.init(
        this.appKioskMode, defaultPrinter, serializedDefaultDestinationRulesStr,
        /** @type {!Array<print_preview.RecentDestination>} */
        (this.getSettingValue('recentDestinations')));
  },

  /** @private */
  onActiveUserChanged_: function() {
    if (!this.activeUser_) {
      return;
    }

    assert(this.cloudPrintState_ !== print_preview.CloudPrintState.DISABLED);
    this.cloudPrintState_ = print_preview.CloudPrintState.SIGNED_IN;

    // Load docs, in case the user was not signed in previously and signed in
    // from the destinations dialog.
    this.destinationStore_.startLoadCookieDestination(
        print_preview.Destination.GooglePromotedId.DOCS);

    // Load any recent cloud destinations for the dropdown.
    const recentDestinations = this.getSettingValue('recentDestinations');
    recentDestinations.forEach(destination => {
      if (destination.origin === print_preview.DestinationOrigin.COOKIES &&
          (destination.account === this.activeUser_ ||
           destination.account === '')) {
        this.destinationStore_.startLoadCookieDestination(destination.id);
      }
    });

    if (this.destinationState === print_preview.DestinationState.SELECTED &&
        this.destination.origin === print_preview.DestinationOrigin.COOKIES) {
      // Adjust states if the destination is now ready to be printed to.
      this.destinationState = this.destination.capabilities ?
          print_preview.DestinationState.UPDATED :
          print_preview.DestinationState.SET;
    }
    this.updateDropdownDestinations_();
  },

  /** @private */
  onDestinationSelect_: function() {
    if (this.state === print_preview_new.State.FATAL_ERROR) {
      // Don't let anything reset if there is a fatal error.
      return;
    }
    const destination = this.destinationStore_.selectedDestination;
    if (this.cloudPrintState_ === print_preview.CloudPrintState.SIGNED_IN ||
        destination.origin !== print_preview.DestinationOrigin.COOKIES) {
      this.destinationState = print_preview.DestinationState.SET;
    } else {
      this.destinationState = print_preview.DestinationState.SELECTED;
    }
    // Notify observers that the destination is set only after updating the
    // destinationState.
    this.destination = destination;
    this.updateRecentDestinations_();
    if (this.cloudPrintState_ === print_preview.CloudPrintState.ENABLED) {
      // Only try to load the docs destination for now. If this request
      // succeeds, it will trigger a transition to SIGNED_IN, and we can
      // load the remaining destinations. Otherwise, it will transition to
      // NOT_SIGNED_IN, so we will not do this more than once.
      this.destinationStore_.startLoadCookieDestination(
          print_preview.Destination.GooglePromotedId.DOCS);
    }
  },

  /** @private */
  onDestinationCapabilitiesReady_: function() {
    this.notifyPath('destination.capabilities');
    if (this.destinationState === print_preview.DestinationState.SET) {
      this.destinationState = print_preview.DestinationState.UPDATED;
    }
    this.updateRecentDestinations_();
  },

  /**
   * @param {!CustomEvent<!print_preview.DestinationErrorType>} e
   * @private
   */
  onDestinationError_: function(e) {
    let errorType = print_preview_new.Error.NONE;
    switch (e.detail) {
      case print_preview.DestinationErrorType.INVALID:
        errorType = print_preview_new.Error.INVALID_PRINTER;
        break;
      case print_preview.DestinationErrorType.UNSUPPORTED:
        errorType = print_preview_new.Error.UNSUPPORTED_PRINTER;
        break;
      // <if expr="chromeos">
      case print_preview.DestinationErrorType.NO_DESTINATIONS:
        errorType = print_preview_new.Error.NO_DESTINATIONS;
        this.noDestinations_ = true;
        break;
      // </if>
      default:
        break;
    }
    this.error = errorType;
    this.destinationState = print_preview.DestinationState.ERROR;
  },

  /**
   * Updates the cloud print status to NOT_SIGNED_IN if there is an
   * authentication error.
   * @param {!CustomEvent<!cloudprint.CloudPrintInterfaceErrorEventDetail>}
   *     event Contains the error status
   * @private
   */
  checkCloudPrintStatus_: function(event) {
    if (event.detail.status != 403 || this.appKioskMode) {
      return;
    }

    // Should not have sent a message to Cloud Print if cloud print is
    // disabled.
    assert(this.cloudPrintState_ !== print_preview.CloudPrintState.DISABLED);
    this.cloudPrintState_ = print_preview.CloudPrintState.NOT_SIGNED_IN;
    console.warn('Google Cloud Print Error: HTTP status 403');
  },

  /**
   * @param {!print_preview.RecentDestination} destination
   * @return {boolean} Whether the destination is Save as PDF or Save to Drive.
   */
  destinationIsDriveOrPdf_: function(destination) {
    return destination.id ===
        print_preview.Destination.GooglePromotedId.SAVE_AS_PDF ||
        destination.id === print_preview.Destination.GooglePromotedId.DOCS;
  },

  /** @private */
  updateRecentDestinations_: function() {
    if (!this.destination) {
      return;
    }

    // Determine if this destination is already in the recent destinations,
    // and where in the array it is located.
    const newDestination =
        print_preview.makeRecentDestination(assert(this.destination));
    const recentDestinations = this.getSettingValue('recentDestinations');
    let indexFound = recentDestinations.findIndex(function(recent) {
      return (
          newDestination.id == recent.id &&
          newDestination.origin == recent.origin);
    });

    // No change
    if (indexFound == 0 &&
        recentDestinations[0].capabilities == newDestination.capabilities) {
      return;
    }
    const isNew = indexFound == -1;

    // Shift the array so that the nth most recent destination is located at
    // index n.
    if (isNew && recentDestinations.length == NUM_PERSISTED_DESTINATIONS) {
      indexFound = NUM_PERSISTED_DESTINATIONS - 1;
    }
    if (indexFound != -1) {
      this.setSettingSplice('recentDestinations', indexFound, 1, null);
    }

    // Add the most recent destination
    this.setSettingSplice('recentDestinations', 0, 0, newDestination);
    if (!this.destinationIsDriveOrPdf_(newDestination) && isNew) {
      this.updateDropdownDestinations_();
    }
  },

  /** @private */
  updateDropdownDestinations_: function() {
    const recentDestinations = this.getSettingValue('recentDestinations');

    const dropdownDestinations = [];
    recentDestinations.forEach(recentDestination => {
      const key = print_preview.createRecentDestinationKey(recentDestination);
      const destination = this.destinationStore_.getDestinationByKey(key);
      if (destination && !this.destinationIsDriveOrPdf_(recentDestination) &&
          (!destination.account || destination.account == this.activeUser_)) {
        dropdownDestinations.push(destination);
      }
    });

    this.recentDestinationList_ = dropdownDestinations;
  },

  /**
   * @return {boolean} Whether the destinations dropdown should be disabled.
   * @private
   */
  shouldDisableDropdown_: function() {
    return this.state === print_preview_new.State.FATAL_ERROR ||
        (this.destinationState === print_preview.DestinationState.UPDATED &&
         this.disabled && this.state !== print_preview_new.State.NOT_READY);
  },

  /** @private */
  computeShouldHideSpinner_: function() {
    return this.destinationState === print_preview.DestinationState.ERROR ||
        this.destinationState === print_preview.DestinationState.UPDATED ||
        (this.destinationState === print_preview.DestinationState.SET &&
         !!this.destination &&
         (!!this.destination.capabilities ||
          this.destination.id ===
              print_preview.Destination.GooglePromotedId.SAVE_AS_PDF));
  },

  /**
   * @return {string} The connection status text to display.
   * @private
   */
  computeStatusText_: function() {
    // |destination| can be either undefined, or null here.
    if (!this.destination) {
      return '';
    }

    return this.destination.shouldShowInvalidCertificateError ?
        this.i18n('noLongerSupportedFragment') :
        this.destination.connectionStatusText;
  },

  // <if expr="chromeos">
  /**
   * @return {boolean}
   * @private
   */
  computeHasPinSetting_: function() {
    return this.getSetting('pin').available;
  },
  // </if>

  /**
   * @param {!CustomEvent<string>} e Event containing the new selected value.
   * @private
   */
  onSelectedDestinationOptionChange_: function(e) {
    const value = e.detail;
    if (value === 'seeMore') {
      this.destinationStore_.startLoadAllDestinations();
      if (this.activeUser_) {
        this.invitationStore_.startLoadingInvitations(this.activeUser_);
      }
      this.$.destinationDialog.get().show();
    } else {
      this.destinationStore_.selectDestinationByKey(value);
    }
  },

  /**
   * @param {!CustomEvent<string>} e Event containing the new active user
   *     account.
   * @private
   */
  onAccountChange_: function(e) {
    this.$.userInfo.updateActiveUser(e.detail);
  },

  /** @private */
  onDialogClose_: function() {
    // Reset the select value if the user dismissed the dialog without
    // selecting a new destination.
    this.updateDestinationSelect_();
    this.$.destinationSelect.focus();
  },

  /** @private */
  updateDestinationSelect_: function() {
    // <if expr="chromeos">
    if (this.destinationState === print_preview.DestinationState.ERROR &&
        !this.destination) {
      return;
    }
    // </if>

    if (this.destinationState === print_preview.DestinationState.INIT ||
        this.destinationState === print_preview.DestinationState.SELECTED) {
      return;
    }

    // TODO (rbpotter): Remove this conditional when the Polymer 2 migration
    // is completed.
    if (Polymer.DomIf) {
      Polymer.RenderStatus.beforeNextRender(this.$.destinationSelect, () => {
        this.$.destinationSelect.updateDestination();
      });
    } else {
      this.$.destinationSelect.async(() => {
        this.$.destinationSelect.updateDestination();
      });
    }
  },
});
})();
