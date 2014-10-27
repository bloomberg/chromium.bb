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
goog.provide('i18n.input.chrome.inputview.content.compact.util');
goog.provide('i18n.input.chrome.inputview.content.compact.util.CompactKeysetSpec');

goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.content.constants');
goog.require('i18n.input.chrome.inputview.elements.ElementType');

goog.scope(function() {
var util = i18n.input.chrome.inputview.content.compact.util;
var Css = i18n.input.chrome.inputview.Css;


/**
 * The compact layout keyset type.
 *
 * @enum {string}
 */
util.CompactKeysetSpec = {
  ID: 'id',
  LAYOUT: 'layout',
  DATA: 'data'
};


/**
 * @desc The name of the layout providing numbers from 0-9 and common
 *     symbols.
 */
util.MSG_NUMBER_AND_SYMBOL =
    goog.getMsg('number and symbol layout');


/**
 * @desc The name of the layout providing more symbols.
 */
util.MSG_MORE_SYMBOLS =
    goog.getMsg('more symbols layout');


/**
 * @desc The name of the main layout.
 */
util.MSG_MAIN_LAYOUT = goog.getMsg('main layout');


/**
 * @desc The name of the english main layout.
 */
util.MSG_ENG_MAIN_LAYOUT = goog.getMsg('english main layout');


/**
 * @desc The name of the english layout providing numbers from 0-9 and common.
 */
util.MSG_ENG_MORE_SYMBOLS =
    goog.getMsg('english more symbols layout');


/**
 * @desc The name of the english layout providing more symbols.
 */
util.MSG_ENG_NUMBER_AND_SYMBOL =
    goog.getMsg('english number and symbol layout');


/**
 * Creates the compact key data.
 *
 * @param {!Object} keysetSpec The keyset spec.
 * @param {string} viewIdPrefix The prefix of the view.
 * @param {string} keyIdPrefix The prefix of the key.
 * @return {!Object} The key data.
 */
util.createCompactData = function(keysetSpec, viewIdPrefix, keyIdPrefix) {
  var keyList = [];
  var mapping = {};
  var keysetSpecNode = util.CompactKeysetSpec;
  for (var i = 0; i < keysetSpec[keysetSpecNode.DATA].length; i++) {
    var keySpec = keysetSpec[keysetSpecNode.DATA][i];
    if (keySpec ==
        i18n.input.chrome.inputview.content.constants.NonLetterKeys.MENU) {
      keySpec['toKeyset'] = keysetSpec[keysetSpecNode.ID].split('.')[0];
    }
    var id = keySpec['id'] ? keySpec['id'] : keyIdPrefix + i;
    var key = util.createCompactKey(id, keySpec);
    keyList.push(key);
    mapping[key['spec']['id']] = viewIdPrefix + i;
  }

  return {
    'keyList': keyList,
    'mapping': mapping,
    'layout': keysetSpec[keysetSpecNode.LAYOUT]
  };
};


/**
 * Creates a key in the compact keyboard.
 *
 * @param {string} id The id.
 * @param {!Object} keySpec The specification.
 * @return {!Object} The compact key.
 */
util.createCompactKey = function(id, keySpec) {
  var spec = keySpec;
  spec['id'] = id;
  if (!spec['type'])
    spec['type'] = i18n.input.chrome.inputview.elements.ElementType.COMPACT_KEY;

  var newSpec = {};
  for (var key in spec) {
    newSpec[key] = spec[key];
  }

  return {
    'spec': newSpec
  };
};


/**
 * Customize the switcher keys in key characters.
 *
 * @param {!Array.<!Object>} keyCharacters The key characters.
 * @param {!Array.<!Object>} switcherKeys The customized switcher keys.
 */
util.customizeSwitchers = function(keyCharacters, switcherKeys) {
  var j = 0;
  for (var i = 0; i < keyCharacters.length; i++) {
    if (keyCharacters[i] ==
        i18n.input.chrome.inputview.content.constants.NonLetterKeys.SWITCHER) {
      if (j >= switcherKeys.length) {
        console.error('The number of switcher key spec is less than' +
            ' the number of switcher keys in the keyset.');
        return;
      }
      var newSpec = switcherKeys[j++];
      for (var key in keyCharacters[i]) {
        newSpec[key] = keyCharacters[i][key];
      }
      keyCharacters[i] = newSpec;
    }
  }
  if (j < switcherKeys.length - 1) {
    console.error('The number of switcher key spec is more than' +
        ' the number of switcher keys in the keyset.');
  }
};


/**
 * Generates letter, symbol and more compact keysets and load them.
 *
 * @param {!Object} letterKeysetSpec The spec of letter keyset.
 * @param {!Object} symbolKeysetSpec The spec of symbol keyset.
 * @param {!Object} moreKeysetSpec The spec of more keyset.
 * @param {!function(!Object): void} onLoaded The function to call once a keyset
 *     data is ready.
 */
util.generateCompactKeyboard =
    function(letterKeysetSpec, symbolKeysetSpec, moreKeysetSpec, onLoaded) {
  // Creates compacty qwerty keyset.
  var keysetSpecNode = util.CompactKeysetSpec;
  util.customizeSwitchers(
      letterKeysetSpec[keysetSpecNode.DATA],
      [{ 'name': '?123',
         'toKeyset': symbolKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_NUMBER_AND_SYMBOL
       }]);

  var data = util.createCompactData(
      letterKeysetSpec, 'compactkbd-k-', 'compactkbd-k-key-');
  data['id'] = letterKeysetSpec[keysetSpecNode.ID];
  data['showMenuKey'] = true;
  onLoaded(data);

  // Creates compacty symbol keyset.
  util.customizeSwitchers(
      symbolKeysetSpec[keysetSpecNode.DATA],
      [{ 'name': '~[<',
         'toKeyset': moreKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_MORE_SYMBOLS
       },
       { 'name': '~[<',
         'toKeyset': moreKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_MORE_SYMBOLS
       },
       { 'name': 'abc',
         'toKeyset': letterKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_MAIN_LAYOUT
       }]);
  data = util.createCompactData(
      symbolKeysetSpec, 'compactkbd-k-', 'compactkbd-k-key-');
  data['id'] = symbolKeysetSpec[keysetSpecNode.ID];
  data['showMenuKey'] = false;
  data['noShift'] = true;
  onLoaded(data);

  // Creates compact more keyset.
  util.customizeSwitchers(
      moreKeysetSpec[keysetSpecNode.DATA],
      [{ 'name': '?123',
         'toKeyset': symbolKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_NUMBER_AND_SYMBOL
       },
       { 'name': '?123',
         'toKeyset': symbolKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_NUMBER_AND_SYMBOL
       },
       { 'name': 'abc',
         'toKeyset': letterKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_MAIN_LAYOUT
       }]);
  data = util.createCompactData(moreKeysetSpec, 'compactkbd-k-',
      'compactkbd-k-key-');
  data['id'] = moreKeysetSpec[keysetSpecNode.ID];
  data['showMenuKey'] = false;
  data['noShift'] = true;
  onLoaded(data);
};


/**
 * Generates letter, symbol and more compact keysets for
 *     pinyin's chinese and english mode and load them.
 *
 * @param {!Object} letterKeysetSpec The spec of letter keyset.
 * @param {!Object} engKeysetSpec The english spec of letter keyset
 * @param {!Object} symbolKeysetSpec The spec of symbol keyset.
 * @param {!Object} engSymbolKeysetSpec The spec of engish symbol keyset.
 * @param {!Object} moreKeysetSpec The spec of more keyset.
 * @param {!Object} engMoreKeysetSpec The spec of english more keyset.
 * @param {!function(!Object): void} onLoaded The function to call once a keyset
 *     data is ready.
 */
util.generatePinyinCompactKeyboard = function(letterKeysetSpec, engKeysetSpec,
    symbolKeysetSpec, engSymbolKeysetSpec,
    moreKeysetSpec, engMoreKeysetSpec, onLoaded) {
  // Creates compacty qwerty keyset for pinyin.
  var keysetSpecNode = util.CompactKeysetSpec;
  util.customizeSwitchers(
      letterKeysetSpec[keysetSpecNode.DATA],
      [{ 'name': '?123',
         'toKeyset': symbolKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_NUMBER_AND_SYMBOL
       },
       { 'toKeyset': engKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_ENG_MAIN_LAYOUT,
         'iconCssClass': Css.SWITCHER_CHINESE
       }]);

  var data = util.createCompactData(
      letterKeysetSpec, 'compactkbd-k-', 'compactkbd-k-key-');
  data['id'] = letterKeysetSpec[keysetSpecNode.ID];
  data['showMenuKey'] = true;
  onLoaded(data);

  // Creates the compacty qwerty keyset for pinyin's English mode.
  util.customizeSwitchers(
      engKeysetSpec[keysetSpecNode.DATA],
      [{ 'name': '?123',
         'toKeyset': engSymbolKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_ENG_NUMBER_AND_SYMBOL
       },
       { 'toKeyset': letterKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_MAIN_LAYOUT,
         'iconCssClass': Css.SWITCHER_ENGLISH
       }]);

  data = util.createCompactData(
      engKeysetSpec, 'compactkbd-k-', 'compactkbd-k-key-');
  data['id'] = engKeysetSpec[keysetSpecNode.ID];
  data['showMenuKey'] = true;
  onLoaded(data);

  // Creates compacty symbol keyset for pinyin.
  util.customizeSwitchers(
      symbolKeysetSpec[keysetSpecNode.DATA],
      [{ 'name': '~[<',
         'toKeyset': moreKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_MORE_SYMBOLS
       },
       { 'name': '~[<',
         'toKeyset': moreKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_MORE_SYMBOLS
       },
       { 'name': 'abc',
         'toKeyset': letterKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_MAIN_LAYOUT
       }]);
  data = util.createCompactData(
      symbolKeysetSpec, 'compactkbd-k-', 'compactkbd-k-key-');
  data['id'] = symbolKeysetSpec[keysetSpecNode.ID];
  data['showMenuKey'] = false;
  data['noShift'] = true;
  onLoaded(data);

  // Creates compacty symbol keyset for English mode.
  util.customizeSwitchers(
      engSymbolKeysetSpec[keysetSpecNode.DATA],
      [{ 'name': '~[<',
         'toKeyset': engMoreKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_ENG_MORE_SYMBOLS
       },
       { 'name': '~[<',
         'toKeyset': engMoreKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_ENG_MORE_SYMBOLS
       },
       { 'name': 'abc',
         'toKeyset': engKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_ENG_MAIN_LAYOUT
       }]);
  data = util.createCompactData(
      engSymbolKeysetSpec, 'compactkbd-k-', 'compactkbd-k-key-');
  data['id'] = engSymbolKeysetSpec[keysetSpecNode.ID];
  data['showMenuKey'] = false;
  data['noShift'] = true;
  onLoaded(data);

  // Creates compact more keyset for pinyin.
  util.customizeSwitchers(
      moreKeysetSpec[keysetSpecNode.DATA],
      [{ 'name': '?123',
         'toKeyset': symbolKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_NUMBER_AND_SYMBOL
       },
       { 'name': '?123',
         'toKeyset': symbolKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_NUMBER_AND_SYMBOL
       },
       { 'name': 'abc',
         'toKeyset': letterKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_MAIN_LAYOUT
       }]);
  data = util.createCompactData(moreKeysetSpec, 'compactkbd-k-',
      'compactkbd-k-key-');
  data['id'] = moreKeysetSpec[keysetSpecNode.ID];
  data['showMenuKey'] = false;
  data['noShift'] = true;
  onLoaded(data);

  // Creates the compact more keyset of english mode.
  util.customizeSwitchers(
      engMoreKeysetSpec[keysetSpecNode.DATA],
      [{ 'name': '?123',
         'toKeyset': engSymbolKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_ENG_NUMBER_AND_SYMBOL
       },
       { 'name': '?123',
         'toKeyset': engSymbolKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_ENG_NUMBER_AND_SYMBOL
       },
       { 'name': 'abc',
         'toKeyset': engKeysetSpec[keysetSpecNode.ID],
         'toKeysetName': util.MSG_ENG_MAIN_LAYOUT
       }]);
  data = util.createCompactData(engMoreKeysetSpec, 'compactkbd-k-',
      'compactkbd-k-key-');
  data['id'] = engMoreKeysetSpec[keysetSpecNode.ID];
  data['showMenuKey'] = false;
  data['noShift'] = true;
  onLoaded(data);
};
});  // goog.scope
