// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and modifying a list of cellular
 * access points.
 */

const kDefaultAccessPointName = 'NONE';
const kOtherAccessPointName = 'Other';

Polymer({
  is: 'network-apnlist',

  behaviors: [I18nBehavior],

  properties: {
    /** @private {!chromeos.networkConfig.mojom.ManagedProperties|undefined} */
    managedProperties: {
      type: Object,
      observer: 'managedPropertiesChanged_',
    },

    /**
     * The name property of the selected APN. If a name property is empty, the
     * accessPointName property will be used. We use 'name' so that multiple
     * APNs with the same accessPointName can be supported, so long as they have
     * a unique 'name' property. This is necessary to allow custom  'other'
     * entries (which are always named 'Other') that match an existing
     * accessPointName but provide a different username/password.
     * @private
     */
    selectedApn_: {
      type: String,
      value: '',
    },

    /**
     * Selectable list of APN dictionaries for the UI. Includes an entry
     * corresponding to |otherApn| (see below).
     * @private {!Array<!chromeos.networkConfig.mojom.ApnProperties>}
     */
    apnSelectList_: {
      type: Array,
      value() {
        return [];
      }
    },

    /**
     * The user settable properties for a new ('other') APN. The values for
     * accessPointName, username, and password will be set to the currently
     * active APN if it does not match an existing list entry.
     * @private {!chromeos.networkConfig.mojom.ApnProperties}
     */
    otherApn_: {
      type: Object,
      value() {
        return {
          accessPointName: kDefaultAccessPointName,
          name: kOtherAccessPointName,
        };
      }
    },

    /**
     * Array of property names to pass to the Other APN property list.
     * @private {!Array<string>}
     */
    otherApnFields_: {
      type: Array,
      value() {
        return ['accessPointName', 'username', 'password'];
      },
      readOnly: true
    },

    /**
     * Array of edit types to pass to the Other APN property list.
     * @private
     */
    otherApnEditTypes_: {
      type: Object,
      value() {
        return {
          'accessPointName': 'String',
          'username': 'String',
          'password': 'Password'
        };
      },
      readOnly: true
    },
  },

  /*
   * Returns the select APN SelectElement.
   * @return {?HTMLSelectElement}
   */
  getApnSelect() {
    return /** @type {?HTMLSelectElement} */ (this.$$('#selectApn'));
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedApnProperties} apn
   * @return {!chromeos.networkConfig.mojom.ApnProperties}
   * @private
   */
  getApnFromManaged_(apn) {
    return {
      // authentication and language are ignored in this UI.
      accessPointName: OncMojo.getActiveString(apn.accessPointName),
      localizedName: OncMojo.getActiveString(apn.localizedName),
      name: OncMojo.getActiveString(apn.name),
      password: OncMojo.getActiveString(apn.password),
      username: OncMojo.getActiveString(apn.username),
    };
  },

  /** @private*/
  getActiveApnFromProperties_(managedProperties) {
    const cellular = managedProperties.typeProperties.cellular;
    /** @type {!chromeos.networkConfig.mojom.ApnProperties|undefined} */ let
        activeApn;
    // We show selectedAPN as the active entry in the select list but it may
    // not correspond to the currently "active" APN which is represented by
    // lastGoodApn.
    if (cellular.selectedApn) {
      activeApn = this.getApnFromManaged_(cellular.selectedApn);
    } else if (cellular.lastGoodApn && cellular.lastGoodApn.accessPointName) {
      activeApn = cellular.lastGoodApn;
    }
    if (activeApn && !activeApn.accessPointName) {
      activeApn = undefined;
    }
    return activeApn;
  },

  /** @private*/
  shouldUpdateSelectList_(oldManagedProperties) {
    if (!oldManagedProperties) {
      return true;
    }

    const newActiveApn =
        this.getActiveApnFromProperties_(this.managedProperties);
    const oldActiveApn = this.getActiveApnFromProperties_(oldManagedProperties);
    if (!OncMojo.apnMatch(newActiveApn, oldActiveApn)) {
      return true;
    }

    const newApnList = this.managedProperties.typeProperties.cellular.apnList;
    const oldApnList = oldManagedProperties.typeProperties.cellular.apnList;
    if (!OncMojo.apnListMatch(
            oldApnList && oldApnList.activeValue,
            newApnList && newApnList.activeValue)) {
      return true;
    }

    const newCustomApnList =
        this.managedProperties.typeProperties.cellular.customApnList;
    const oldCustomApnList =
        oldManagedProperties.typeProperties.cellular.customApnList;
    if (!OncMojo.apnListMatch(oldCustomApnList, newCustomApnList)) {
      return true;
    }

    return false;
  },

  /** @private*/
  managedPropertiesChanged_(managedProperties, oldManagedProperties) {
    if (!this.shouldUpdateSelectList_(oldManagedProperties)) {
      return;
    }
    this.setApnSelectList_(this.getActiveApnFromProperties_(managedProperties));
  },

  /**
   * Sets the list of selectable APNs for the UI. Appends an 'Other' entry
   * (see comments for |otherApn_| above).
   * @param {chromeos.networkConfig.mojom.ApnProperties|undefined} activeApn
   * @private
   */
  setApnSelectList_(activeApn) {
    assert(!activeApn || activeApn.accessPointName);
    // The generated APN list ensures nonempty accessPointName and name
    // properties.
    const apnList = this.generateApnList_();
    if (apnList === undefined) {
      // No APNList property indicates that the network is not in a
      // connectable state. Disable the UI.
      this.apnSelectList_ = [];
      this.set('selectedApn_', '');
      return;
    }
    // Get the list entry for activeApn if it exists. It will have 'name' set.
    let activeApnInList;
    if (activeApn) {
      activeApnInList = apnList.find(a => a.name === activeApn.name);
    }

    const customApnList =
        this.managedProperties.typeProperties.cellular.customApnList;
    let otherApn = this.otherApn_;
    if (customApnList && customApnList.length) {
      // If custom apn list exists, then use it's first entry as otherApn.
      otherApn = customApnList[0];
    } else if (!activeApnInList && activeApn && activeApn.accessPointName) {
      // If the active APN is not in the list, copy it to otherApn.
      otherApn = activeApn;
    }
    this.otherApn_ = {
      accessPointName: otherApn.accessPointName,
      name: kOtherAccessPointName,
      username: otherApn.username,
      password: otherApn.password,
    };
    apnList.push(this.otherApn_);

    this.apnSelectList_ = apnList;
    const selectedApn =
        activeApnInList ? activeApnInList.name : kOtherAccessPointName;
    assert(selectedApn);
    this.set('selectedApn_', selectedApn);

    // Wait for the dom-repeat to populate the <option> entries then explicitly
    // set the selected value.
    this.async(function() {
      this.$.selectApn.value = this.selectedApn_;
    });
  },

  /**
   * Returns a modified copy of the APN properties or undefined if the
   * property is not set. All entries in the returned copy will have nonempty
   * name and accessPointName properties.
   * @return {!Array<!chromeos.networkConfig.mojom.ApnProperties>|undefined}
   * @private
   */
  generateApnList_() {
    if (!this.managedProperties) {
      return undefined;
    }
    const apnList = this.managedProperties.typeProperties.cellular.apnList;
    if (!apnList) {
      return undefined;
    }
    return apnList.activeValue.filter(apn => !!apn.accessPointName).map(apn => {
      return {
        accessPointName: apn.accessPointName,
        localizedName: apn.localizedName,
        name: apn.name || apn.accessPointName,
        username: apn.username,
        password: apn.password,
      };
    });
  },

  /**
   * Event triggered when the selectApn selection changes.
   * @param {!Event} event
   * @private
   */
  onSelectApnChange_(event) {
    const target = /** @type {!HTMLSelectElement} */ (event.target);
    const name = target.value;
    // When selecting 'Other', don't send a change event unless a valid
    // non-default value has been set for Other.
    if (name === kOtherAccessPointName &&
        (!this.otherApn_.accessPointName ||
         this.otherApn_.accessPointName === kDefaultAccessPointName)) {
      this.selectedApn_ = name;
      return;
    }
    // The change will generate an update which will update selectedApn_ and
    // refresh the UI.
    this.sendApnChange_(name);
  },

  /**
   * Event triggered when any 'Other' APN network property changes.
   * @param {!CustomEvent<!{field: string, value: string}>} event
   * @private
   */
  onOtherApnChange_(event) {
    // TODO(benchan/stevenjb): Move the toUpperCase logic to shill or
    // onc_translator_onc_to_shill.cc.
    const value = (event.detail.field === 'accessPointName') ?
        event.detail.value.toUpperCase() :
        event.detail.value;
    this.set('otherApn_.' + event.detail.field, value);
    // Don't send a change event for 'Other' until the 'Save' button is tapped.
  },

  /**
   * Event triggered when the Other APN 'Save' button is tapped.
   * @param {!Event} event
   * @private
   */
  onSaveOtherTap_(event) {
    this.sendApnChange_(this.selectedApn_);
  },

  /**
   * Send the apn-change event.
   * @param {string} name The APN name property.
   * @private
   */
  sendApnChange_(name) {
    let apn;
    if (name === kOtherAccessPointName) {
      if (!this.otherApn_.accessPointName ||
          this.otherApn_.accessPointName === kDefaultAccessPointName) {
        // No valid APN set, do nothing.
        return;
      }
      apn = {
        accessPointName: this.otherApn_.accessPointName,
        username: this.otherApn_.username,
        password: this.otherApn_.password,
      };
    } else {
      apn = this.apnSelectList_.find(a => a.name === name);
      if (apn === undefined) {
        // Potential edge case if an update is received before this is invoked.
        console.error('Selected APN not in list');
        return;
      }
    }
    this.fire('apn-change', apn);
  },

  /**
   * @return {boolean}
   * @private
   */
  isDisabled_() {
    return this.selectedApn_ === '';
  },

  /**
   * @return {boolean}
   * @private
   */
  showOtherApn_() {
    return this.selectedApn_ === kOtherAccessPointName;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ApnProperties} apn
   * @return {string} The most descriptive name for the access point.
   * @private
   */
  apnDesc_(apn) {
    assert(apn.name);
    return apn.localizedName || apn.name;
  },

  /**
   * @param {chromeos.networkConfig.mojom.ApnProperties} item
   * @return {boolean} Boolean indicating whether |item| is the current selected
   *     apn item.
   * @private
   */
  isApnItemSelected_(item) {
    return item.accessPointName === this.selectedApn_;
  }
});
