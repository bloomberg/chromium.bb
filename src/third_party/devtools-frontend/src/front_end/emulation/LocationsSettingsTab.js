// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../common/common.js';
import {ls} from '../platform/platform.js';
import * as UI from '../ui/ui.js';

/** @type {!LocationsSettingsTab} */
let locationsSettingsTabInstance;

/**
 * @implements {UI.ListWidget.Delegate<Item>}
 */
export class LocationsSettingsTab extends UI.Widget.VBox {
  /** @private */
  constructor() {
    super(true);
    this.registerRequiredCSS('emulation/locationsSettingsTab.css', {enableLegacyPatching: true});

    this.contentElement.createChild('div', 'header').textContent = Common.UIString.UIString('Custom locations');

    const addButton = UI.UIUtils.createTextButton(
        Common.UIString.UIString('Add location\u2026'), this._addButtonClicked.bind(this), 'add-locations-button');
    this.contentElement.appendChild(addButton);

    this._list = new UI.ListWidget.ListWidget(this);
    this._list.element.classList.add('locations-list');
    this._list.registerRequiredCSS('emulation/locationsSettingsTab.css', {enableLegacyPatching: true});
    this._list.show(this.contentElement);
    /** @type {Common.Settings.Setting<Array<LocationDescription>>} */
    this._customSetting = /** @type {Common.Settings.Setting<Array<LocationDescription>>} */ (
        Common.Settings.Settings.instance().moduleSetting('emulation.locations'));
    const list = /** @type {Array<*>} */ (this._customSetting.get())
                     .map(location => replaceLocationTitles(location, this._customSetting.defaultValue()));

    /**
       * @param {LocationDescription} location
       * @param {Array<LocationDescription>} defaultValues
       */
    function replaceLocationTitles(location, defaultValues) {
      // This check is done for locations that might had been cached wrongly due to crbug.com/1171670.
      // Each of the default values would have been stored without a title if the user had added a new location
      // while the bug was present in the application. This means that getting the setting's default value with the `get`
      // method would return the default locations without a title. To cope with this, the setting values are
      // preemptively checked and corrected so that any default value mistakenly stored without a title is replaced
      // with the corresponding declared value in the pre-registered setting.
      if (!location.title) {
        const replacement = defaultValues.find(
            defaultLocation => defaultLocation.lat === location.lat && defaultLocation.long === location.long &&
                defaultLocation.timezoneId === location.timezoneId && defaultLocation.locale === location.locale);
        if (!replacement) {
          console.error('Could not determine a location setting title');
        } else {
          return replacement;
        }
      }
      return location;
    }

    this._customSetting.set(list);
    this._customSetting.addChangeListener(this._locationsUpdated, this);

    this.setDefaultFocusedElement(addButton);
  }

  static instance() {
    if (!locationsSettingsTabInstance) {
      locationsSettingsTabInstance = new LocationsSettingsTab();
    }

    return locationsSettingsTabInstance;
  }

  /**
   * @override
   */
  wasShown() {
    super.wasShown();
    this._locationsUpdated();
  }

  _locationsUpdated() {
    this._list.clear();

    const conditions = this._customSetting.get();
    for (const condition of conditions) {
      this._list.appendItem(condition, true);
    }

    this._list.appendSeparator();
  }

  _addButtonClicked() {
    this._list.addNewItem(this._customSetting.get().length, {title: '', lat: 0, long: 0, timezoneId: '', locale: ''});
  }

  /**
   * @override
   * @param {!Item} location
   * @param {boolean} editable
   * @return {!Element}
   */
  renderItem(location, editable) {
    const element = document.createElement('div');
    element.classList.add('locations-list-item');
    const title = element.createChild('div', 'locations-list-text locations-list-title');
    const titleText = title.createChild('div', 'locations-list-title-text');
    titleText.textContent = location.title;
    UI.Tooltip.Tooltip.install(titleText, location.title);
    element.createChild('div', 'locations-list-separator');
    element.createChild('div', 'locations-list-text').textContent = String(location.lat);
    element.createChild('div', 'locations-list-separator');
    element.createChild('div', 'locations-list-text').textContent = String(location.long);
    element.createChild('div', 'locations-list-separator');
    element.createChild('div', 'locations-list-text').textContent = location.timezoneId;
    element.createChild('div', 'locations-list-separator');
    element.createChild('div', 'locations-list-text').textContent = location.locale;
    return element;
  }

  /**
   * @override
   * @param {*} item
   * @param {number} index
   */
  removeItemRequested(item, index) {
    const list = this._customSetting.get();
    list.splice(index, 1);
    this._customSetting.set(list);
  }

  /**
   * @override
   * @param {!Item} location
   * @param {!UI.ListWidget.Editor<!Item>} editor
   * @param {boolean} isNew
   */
  commitEdit(location, editor, isNew) {
    location.title = editor.control('title').value.trim();
    const lat = editor.control('lat').value.trim();
    location.lat = lat ? parseFloat(lat) : 0;
    const long = editor.control('long').value.trim();
    location.long = long ? parseFloat(long) : 0;
    const timezoneId = editor.control('timezoneId').value.trim();
    location.timezoneId = timezoneId;
    const locale = editor.control('locale').value.trim();
    location.locale = locale;

    const list = this._customSetting.get();
    if (isNew) {
      list.push(location);
    }
    this._customSetting.set(list);
  }

  /**
   * @override
   * @param {!Item} location
   * @return {!UI.ListWidget.Editor<!Item>}
   */
  beginEdit(location) {
    const editor = this._createEditor();
    editor.control('title').value = location.title;
    editor.control('lat').value = String(location.lat);
    editor.control('long').value = String(location.long);
    editor.control('timezoneId').value = location.timezoneId;
    editor.control('locale').value = location.locale;
    return editor;
  }

  /**
   * @return {!UI.ListWidget.Editor<!Item>}
   */
  _createEditor() {
    if (this._editor) {
      return this._editor;
    }

    const editor = new UI.ListWidget.Editor();
    this._editor = editor;
    const content = editor.contentElement();

    const titles = content.createChild('div', 'locations-edit-row');
    titles.createChild('div', 'locations-list-text locations-list-title').textContent =
        Common.UIString.UIString('Location name');
    titles.createChild('div', 'locations-list-separator locations-list-separator-invisible');
    titles.createChild('div', 'locations-list-text').textContent = Common.UIString.UIString('Lat');
    titles.createChild('div', 'locations-list-separator locations-list-separator-invisible');
    titles.createChild('div', 'locations-list-text').textContent = Common.UIString.UIString('Long');
    titles.createChild('div', 'locations-list-separator locations-list-separator-invisible');
    titles.createChild('div', 'locations-list-text').textContent = Common.UIString.UIString('Timezone ID');
    titles.createChild('div', 'locations-list-separator locations-list-separator-invisible');
    titles.createChild('div', 'locations-list-text').textContent = Common.UIString.UIString('Locale');

    const fields = content.createChild('div', 'locations-edit-row');
    fields.createChild('div', 'locations-list-text locations-list-title locations-input-container')
        .appendChild(editor.createInput('title', 'text', ls`Location name`, titleValidator));
    fields.createChild('div', 'locations-list-separator locations-list-separator-invisible');

    let cell = fields.createChild('div', 'locations-list-text locations-input-container');
    cell.appendChild(editor.createInput('lat', 'text', ls`Latitude`, latValidator));
    fields.createChild('div', 'locations-list-separator locations-list-separator-invisible');

    cell = fields.createChild('div', 'locations-list-text locations-list-text-longitude locations-input-container');
    cell.appendChild(editor.createInput('long', 'text', ls`Longitude`, longValidator));
    fields.createChild('div', 'locations-list-separator locations-list-separator-invisible');

    cell = fields.createChild('div', 'locations-list-text locations-input-container');
    cell.appendChild(editor.createInput('timezoneId', 'text', ls`Timezone ID`, timezoneIdValidator));
    fields.createChild('div', 'locations-list-separator locations-list-separator-invisible');

    cell = fields.createChild('div', 'locations-list-text locations-input-container');
    cell.appendChild(editor.createInput('locale', 'text', ls`Locale`, localeValidator));

    return editor;

    /**
     * @param {*} item
     * @param {number} index
     * @param {!HTMLInputElement|!HTMLSelectElement} input
     * @return {!UI.ListWidget.ValidatorResult}
     */
    function titleValidator(item, index, input) {
      const maxLength = 50;
      const value = input.value.trim();

      let errorMessage;
      if (!value.length) {
        errorMessage = ls`Location name cannot be empty`;
      } else if (value.length > maxLength) {
        errorMessage = ls`Location name must be less than ${maxLength} characters`;
      }

      if (errorMessage) {
        return {valid: false, errorMessage};
      }
      return {valid: true, errorMessage: undefined};
    }

    /**
     * @param {*} item
     * @param {number} index
     * @param {!HTMLInputElement|!HTMLSelectElement} input
     * @return {!UI.ListWidget.ValidatorResult}
     */
    function latValidator(item, index, input) {
      const minLat = -90;
      const maxLat = 90;
      const value = input.value.trim();
      const parsedValue = Number(value);

      if (!value) {
        return {valid: true, errorMessage: undefined};
      }

      let errorMessage;
      if (Number.isNaN(parsedValue)) {
        errorMessage = ls`Latitude must be a number`;
      } else if (parseFloat(value) < minLat) {
        errorMessage = ls`Latitude must be greater than or equal to ${minLat}`;
      } else if (parseFloat(value) > maxLat) {
        errorMessage = ls`Latitude must be less than or equal to ${maxLat}`;
      }

      if (errorMessage) {
        return {valid: false, errorMessage};
      }
      return {valid: true, errorMessage: undefined};
    }

    /**
     * @param {*} item
     * @param {number} index
     * @param {!HTMLInputElement|!HTMLSelectElement} input
     * @return {!UI.ListWidget.ValidatorResult}
     */
    function longValidator(item, index, input) {
      const minLong = -180;
      const maxLong = 180;
      const value = input.value.trim();
      const parsedValue = Number(value);

      if (!value) {
        return {valid: true, errorMessage: undefined};
      }

      let errorMessage;
      if (Number.isNaN(parsedValue)) {
        errorMessage = ls`Longitude must be a number`;
      } else if (parseFloat(value) < minLong) {
        errorMessage = ls`Longitude must be greater than or equal to ${minLong}`;
      } else if (parseFloat(value) > maxLong) {
        errorMessage = ls`Longitude must be less than or equal to ${maxLong}`;
      }

      if (errorMessage) {
        return {valid: false, errorMessage};
      }
      return {valid: true, errorMessage: undefined};
    }

    /**
     * @param {*} item
     * @param {number} index
     * @param {!HTMLInputElement|!HTMLSelectElement} input
     * @return {!UI.ListWidget.ValidatorResult}
     */
    function timezoneIdValidator(item, index, input) {
      const value = input.value.trim();
      // Chromium uses ICU's timezone implementation, which is very
      // liberal in what it accepts. ICU does not simply use an allowlist
      // but instead tries to make sense of the input, even for
      // weird-looking timezone IDs. There's not much point in validating
      // the input other than checking if it contains at least one
      // alphabetic character. The empty string resets the override,
      // and is accepted as well.
      if (value === '' || /[a-zA-Z]/.test(value)) {
        return {valid: true, errorMessage: undefined};
      }
      const errorMessage = ls`Timezone ID must contain alphabetic characters`;
      return {valid: false, errorMessage};
    }

    /**
     * @param {*} item
     * @param {number} index
     * @param {!HTMLInputElement|!HTMLSelectElement} input
     * @return {!UI.ListWidget.ValidatorResult}
     */
    function localeValidator(item, index, input) {
      const value = input.value.trim();
      // Similarly to timezone IDs, there's not much point in validating
      // input locales other than checking if it contains at least two
      // alphabetic characters.
      // https://unicode.org/reports/tr35/#Unicode_language_identifier
      // The empty string resets the override, and is accepted as
      // well.
      if (value === '' || /[a-zA-Z]{2}/.test(value)) {
        return {valid: true, errorMessage: undefined};
      }
      const errorMessage = ls`Locale must contain alphabetic characters`;
      return {valid: false, errorMessage};
    }
  }
}

/** @typedef {{title: string, lat: number, long: number, timezoneId: string, locale: string}} */
// @ts-ignore typedef
export let Item;

/**
  * @typedef {{
  * title: (undefined | string ),
  * lat: number,
  * long: number,
  * timezoneId: string,
  * locale: string
  * }}
  */
// @ts-ignore typedef
export let LocationDescription;
