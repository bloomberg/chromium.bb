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
goog.provide('i18n.input.chrome.inputview.layouts.SpaceRow');

goog.require('i18n.input.chrome.inputview.ConditionName');
goog.require('i18n.input.chrome.inputview.layouts.util');


/**
 * Creates the spaceKey row.
 *
 * @return {!Object} The spaceKey row.
 */
i18n.input.chrome.inputview.layouts.SpaceRow.create = function() {
  var globeKey = i18n.input.chrome.inputview.layouts.util.createKey({
    'condition': i18n.input.chrome.inputview.ConditionName.SHOW_GLOBE_OR_SYMBOL,
    'widthInWeight': 1
  });
  var menuKey = i18n.input.chrome.inputview.layouts.util.createKey({
    'condition': i18n.input.chrome.inputview.ConditionName.SHOW_MENU,
    'widthInWeight': 1
  });
  var ctrlKey = i18n.input.chrome.inputview.layouts.util.createKey({
    'widthInWeight': 1
  });
  var altKey = i18n.input.chrome.inputview.layouts.util.createKey({
    'widthInWeight': 1
  });
  var spaceKey = i18n.input.chrome.inputview.layouts.util.createKey({
    'widthInWeight': 4.87
  });
  var enSwitcher = i18n.input.chrome.inputview.layouts.util.createKey({
    'widthInWeight': 1,
    'condition': i18n.input.chrome.inputview.ConditionName.
        SHOW_EN_SWITCHER_KEY
  });
  var altGrKey = i18n.input.chrome.inputview.layouts.util.createKey({
    'widthInWeight': 1.25,
    'condition': i18n.input.chrome.inputview.ConditionName.
        SHOW_ALTGR
  });
  // If globeKey or altGrKey is not shown, give its weight to space key.
  globeKey['spec']['giveWeightTo'] = spaceKey['spec']['id'];
  menuKey['spec']['giveWeightTo'] = spaceKey['spec']['id'];
  altGrKey['spec']['giveWeightTo'] = spaceKey['spec']['id'];
  enSwitcher['spec']['giveWeightTo'] = spaceKey['spec']['id'];

  var leftKey = i18n.input.chrome.inputview.layouts.util.createKey({
    'widthInWeight': 1.08
  });
  var rightKey = i18n.input.chrome.inputview.layouts.util.createKey({
    'widthInWeight': 1.08
  });
  var hideKeyboardKey = i18n.input.chrome.inputview.layouts.util.createKey({
    'widthInWeight': 1.08
  });
  var spaceKeyRow = i18n.input.chrome.inputview.layouts.util.
      createLinearLayout({
        'id': 'spaceKeyrow',
        'children': [globeKey, menuKey, ctrlKey, altKey, spaceKey,
             enSwitcher, altGrKey, leftKey, rightKey, hideKeyboardKey]
      });
  return spaceKeyRow;
};

