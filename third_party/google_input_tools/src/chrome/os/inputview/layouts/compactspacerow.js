// Copyright 2014 The ChromeOS IME Authors. All Rights Reserved.
// limitations under the License.
// See the License for the specific language governing permissions and
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// distributed under the License is distributed on an "AS-IS" BASIS,
// Unless required by applicable law or agreed to in writing, software
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// You may obtain a copy of the License at
// you may not use this file except in compliance with the License.
// Licensed under the Apache License, Version 2.0 (the "License");
//
goog.provide('i18n.input.chrome.inputview.layouts.CompactSpaceRow');

goog.require('i18n.input.chrome.inputview.ConditionName');
goog.require('i18n.input.chrome.inputview.layouts.util');


/**
 * Creates the compact keyboard spaceKey row.
 * @param {boolean} isNordic True if the space key row is generated for compact
 *     nordic layout.
 * @return {!Object} The compact spaceKey row.
 */
i18n.input.chrome.inputview.layouts.CompactSpaceRow.create = function(
    isNordic) {
  var digitSwitcher = i18n.input.chrome.inputview.layouts.util.createKey({
    'widthInWeight': 1.1
  });
  var globeOrSymbolKey = i18n.input.chrome.inputview.layouts.util.createKey({
    'condition': i18n.input.chrome.inputview.ConditionName.SHOW_GLOBE_OR_SYMBOL,
    'widthInWeight': 1
  });
  var menuKey = i18n.input.chrome.inputview.layouts.util.createKey({
    'condition': i18n.input.chrome.inputview.ConditionName.SHOW_MENU,
    'widthInWeight': 1
  });
  var slashKey = i18n.input.chrome.inputview.layouts.util.createKey({
    'widthInWeight': 1
  });
  var space;
  if (isNordic) {
    space = i18n.input.chrome.inputview.layouts.util.createKey({
      'widthInWeight': 5
    });
  } else {
    space = i18n.input.chrome.inputview.layouts.util.createKey({
      'widthInWeight': 4
    });
  }
  var comma = i18n.input.chrome.inputview.layouts.util.createKey({
    'widthInWeight': 1
  });
  var period = i18n.input.chrome.inputview.layouts.util.createKey({
    'widthInWeight': 1
  });
  var hide = i18n.input.chrome.inputview.layouts.util.createKey({
    'widthInWeight': 1.1
  });

  menuKey['spec']['giveWeightTo'] = space['spec']['id'];
  globeOrSymbolKey['spec']['giveWeightTo'] = space['spec']['id'];

  var spaceKeyRow = i18n.input.chrome.inputview.layouts.util.
      createLinearLayout({
        'id': 'spaceKeyrow',
        'children': [digitSwitcher, globeOrSymbolKey, menuKey, slashKey, space,
            comma, period, hide]
      });
  return spaceKeyRow;
};
