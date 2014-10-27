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
goog.provide('i18n.input.chrome.inputview.content.compact.symbol');

goog.require('i18n.input.chrome.inputview.content.constants');

goog.scope(function() {
var NonLetterKeys = i18n.input.chrome.inputview.content.constants.NonLetterKeys;


/**
 * Gets North American Symbol keyset characters.
 *
 * @return {!Array.<!Object>}
 */
i18n.input.chrome.inputview.content.compact.symbol.keyNASymbolCharacters =
    function() {
  return [
    /* 0 */ { 'text': '1',
      'moreKeys': ['\u00B9', '\u00BD', '\u2153', '\u00BC', '\u215B']},
    /* 1 */ { 'text': '2',
      'moreKeys': ['\u00B2', '\u2154']},
    /* 2 */ { 'text': '3',
      'moreKeys': ['\u00B3', '\u00BE', '\u215C']},
    /* 3 */ { 'text': '4',
      'moreKeys': ['\u2074']},
    /* 4 */ { 'text': '5',
      'moreKeys': ['\u215D']},
    /* 5 */ { 'text': '6' },
    /* 6 */ { 'text': '7',
      'moreKeys': ['\u215E']},
    /* 7 */ { 'text': '8' },
    /* 8 */ { 'text': '9' },
    /* 9 */ { 'text': '0',
      'moreKeys': ['\u207F', '\u2205']},
    /* 10 */ NonLetterKeys.BACKSPACE,
    /* 11 */ { 'text': '@', 'marginLeftPercent': 0.33 },
    /* 12 */ { 'text': '#' },
    /* 13 */ { 'text': '$',
      'moreKeys': ['\u00A2', '\u00A3', '\u20AC', '\u00A5', '\u20B1']},
    /* 14 */ { 'text': '%',
      'moreKeys': ['\u2030']},
    /* 15 */ { 'text': '&' },
    // Keep in sync with rowkeys_symbols2.xml in android input tool.
    /* 16 */ { 'text': '-',
      'moreKeys': ['_', '\u2013', '\u2014', '\u00B7']},
    /* 17 */ { 'text': '+',
      'moreKeys': ['\u00B1']},
    /* 18 */ { 'text': '(',
      'moreKeys': ['\u007B', '\u003C', '\u005B']},
    /* 19 */ { 'text': ')',
      'moreKeys': ['\u007D', '\u003E', '\u005D']},
    /* 20 */ NonLetterKeys.ENTER,
    /* 21 */ NonLetterKeys.SWITCHER,
    /* 22 */ { 'text': '\\' },
    /* 23 */ { 'text': '=' },
    /* 24 */ { 'text': '*',
      'moreKeys': ['\u2020', '\u2021', '\u2605']},
    // keep in sync with double_lqm_rqm and double_laqm_raqm in android input
    // tool.
    /* 25 */ { 'text': '"',
      'moreKeys': ['\u201C', '\u201E', '\u201D', '\u00AB', '\u00BB']},
    // keep in sync with single_lqm_rqm and single_laqm_raqm in android input
    // tool
    /* 26 */ { 'text': '\'',
      'moreKeys': ['\u2018', '\u201A', '\u2019', '\u2039', '\u203A']},
    /* 27 */ { 'text': ':' },
    /* 28 */ { 'text': ';' },
    /* 29 */ { 'text': '!',
      'moreKeys': ['\u00A1']},
    /* 30 */ { 'text': '?',
      'moreKeys': ['\u00BF']},
    /* 31 */ NonLetterKeys.SWITCHER,
    /* 32 */ NonLetterKeys.SWITCHER,
    /* 33 */ { 'text': '_', 'isGrey': true },
    /* 34 */ NonLetterKeys.MENU,
    /* 35 */ { 'text': '/', 'isGrey': true },
    /* 36 */ NonLetterKeys.SPACE,
    /* 37 */ { 'text': ',', 'isGrey': true },
    /* 38 */ { 'text': '.', 'isGrey': true,
      'moreKeys': ['\u2026']},
    /* 39 */ NonLetterKeys.HIDE
  ];
};


/**
 * Gets United Kingdom Symbol keyset characters.
 *
 * @return {!Array.<!Object>}
 */
i18n.input.chrome.inputview.content.compact.symbol.keyUKSymbolCharacters =
    function() {
  // Copy North America symbol characters.
  var data = i18n.input.chrome.inputview.content.compact.symbol.
      keyNASymbolCharacters();
  // UK uses pound sign instead of dollar sign.
  data[13] = { 'text': '\u00A3',
               'moreKeys': ['\u00A2', '$', '\u20AC', '\u00A5', '\u20B1']};
  return data;
};


/**
 * Gets European Symbol keyset characters.
 *
 * @return {!Array.<!Object>}
 */
i18n.input.chrome.inputview.content.compact.symbol.keyEUSymbolCharacters =
    function() {
  // Copy UK symbol characters.
  var data = i18n.input.chrome.inputview.content.compact.symbol.
      keyUKSymbolCharacters();
  // European uses euro sign instead of pound sign.
  data[13] = { 'text': '\u20AC',
               'moreKeys': ['\u00A2', '$', '\u00A3', '\u00A5', '\u20B1']};
  return data;
};


/**
 * Gets Pinyin Symbol keyset characters.
 *
 * @return {!Array.<!Object>}
 */
i18n.input.chrome.inputview.content.compact.symbol.keyPinyinSymbolCharacters =
    function() {
  var data = i18n.input.chrome.inputview.content.compact.symbol.
      keyNASymbolCharacters();
  data[13]['text'] = '\u00A5';
  data[13]['moreKeys'] = ['\u0024', '\u00A2', '\u00A3', '\u20AC', '\u20B1'];
  data[15]['text'] = '\uff0f';
  data[16]['text'] = '\uff0d';
  data[18]['text'] = '\uff08';
  data[18]['moreKeys'] = ['\uff5b', '\u300a', '\uff3b', '\u3010'];
  data[19]['text'] = '\uff09';
  data[19]['moreKeys'] = ['\uff5d', '\u300b', '\uff3d', '\u3001'];
  data[22]['text'] = '\u3001';
  data[24]['text'] = '\u00D7';
  data[25]['text'] = '\u201C';
  data[25]['moreKeys'] = ['\u0022', '\u00AB'];
  data[26]['text'] = '\u201D';
  data[26]['moreKeys'] = ['\u0022', '\u00BB'];
  data[27]['text'] = '\uff1a';
  data[28]['text'] = '\uff1b';
  data[29]['text'] = '\u2018';
  data[29]['moreKeys'] = ['\u0027', '\u2039'];
  data[30]['text'] = '\u2019';
  data[30]['moreKeys'] = ['\u0027', '\u203a'];
  data[33]['text'] = '\uff01';
  data[33]['moreKeys'] = ['\u00a1'];
  data[35]['text'] = '\uff1f';
  data[35]['moreKeys'] = ['\u00bf'];
  data[37]['text'] = '\uff0c';
  data[38]['text'] = '\u3002';
  return data;
};
});  // goog.scope
