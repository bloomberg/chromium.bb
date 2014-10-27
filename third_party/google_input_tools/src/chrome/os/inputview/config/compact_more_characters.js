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
goog.provide('i18n.input.chrome.inputview.content.compact.more');

goog.require('i18n.input.chrome.inputview.content.constants');

goog.scope(function() {
var NonLetterKeys = i18n.input.chrome.inputview.content.constants.NonLetterKeys;


/**
 * North American More keyset characters.
 *
 * @return {!Array.<!Object>}
 */
i18n.input.chrome.inputview.content.compact.more.keyNAMoreCharacters =
    function() {
  return [
    /* 0 */ { 'text': '~' },
    /* 1 */ { 'text': '`' },
    /* 2 */ { 'text': '|' },
    // Keep in sync with rowkeys_symbols_shift1.xml in android input tool.
    /* 3 */ { 'text': '\u2022',
      'moreKeys': ['\u266A', '\u2665', '\u2660', '\u2666', '\u2663']},
    /* 4 */ { 'text': '\u23B7' },
    // Keep in sync with rowkeys_symbols_shift1.xml in android input tool.
    /* 5 */ { 'text': '\u03C0',
      'moreKeys': ['\u03A0']},
    /* 6 */ { 'text': '\u00F7' },
    /* 7 */ { 'text': '\u00D7' },
    /* 8 */ { 'text': '\u00B6',
      'moreKeys': ['\u00A7']},
    /* 9 */ { 'text': '\u0394' },
    /* 10 */ NonLetterKeys.BACKSPACE,
    /* 11 */ { 'text': '\u00A3', 'marginLeftPercent': 0.33 },
    /* 12 */ { 'text': '\u00A2' },
    /* 13 */ { 'text': '\u20AC' },
    /* 14 */ { 'text': '\u00A5' },
    // Keep in sync with rowkeys_symbols_shift2.xml in android input tool.
    /* 15 */ { 'text': '^',
      'moreKeys': ['\u2191', '\u2193', '\u2190', '\u2192']},
    /* 16 */ { 'text': '\u00B0',
      'moreKeys': ['\u2032', '\u2033']},
    /* 17 */ { 'text': '=',
      'moreKeys': ['\u2260', '\u2248', '\u221E']},
    /* 18 */ { 'text': '{' },
    /* 19 */ { 'text': '}' },
    /* 20 */ NonLetterKeys.ENTER,
    /* 21 */ NonLetterKeys.SWITCHER,
    /* 22 */ { 'text': '\\' },
    /* 23 */ { 'text': '\u00A9' },
    /* 24 */ { 'text': '\u00AE' },
    /* 25 */ { 'text': '\u2122' },
    /* 26 */ { 'text': '\u2105' },
    /* 27 */ { 'text': '[' },
    /* 28 */ { 'text': ']' },
    /* 29 */ { 'text': '\u00A1' },
    /* 30 */ { 'text': '\u00BF' },
    /* 31 */ NonLetterKeys.SWITCHER,
    /* 32 */ NonLetterKeys.SWITCHER,
    /* 33 */ { 'text': '<', 'isGrey': true,
      'moreKeys': ['\u2039', '\u2264', '\u00AB']},
    /* 34 */ NonLetterKeys.MENU,
    /* 35 */ { 'text': '>', 'isGrey': true,
      'moreKeys': ['\u203A', '\u2265', '\u00BB']},
    /* 36 */ NonLetterKeys.SPACE,
    /* 37 */ { 'text': ',', 'isGrey': true },
    /* 38 */ { 'text': '.', 'isGrey': true,
      'moreKeys': ['\u2026']},
    /* 39 */ NonLetterKeys.HIDE
  ];
};


/**
 * Gets United Kingdom More keyset characters.
 *
 * @return {!Array.<!Object>}
 */
i18n.input.chrome.inputview.content.compact.more.keyUKMoreCharacters =
    function() {
  // Copy North America more characters.
  var data = i18n.input.chrome.inputview.content.compact.more.
      keyNAMoreCharacters();
  data[11]['text'] = '\u20AC';  // pound -> euro
  data[12]['text'] = '\u00A5';  // cent -> yen
  data[13]['text'] = '$';  // euro -> dollar
  data[13]['moreKeys'] = ['\u00A2'];
  data[14]['text'] = '\u00A2';  // yen -> cent
  return data;
};


/**
 * Gets European More keyset characters.
 *
 * @return {!Array.<!Object>}
 */
i18n.input.chrome.inputview.content.compact.more.keyEUMoreCharacters =
    function() {
  // Copy UK more characters.
  var data = i18n.input.chrome.inputview.content.compact.more.
      keyUKMoreCharacters();
  data[11]['text'] = '\u00A3';  // euro -> pound
  return data;
};


/**
 * Gets Pinyin More keyset characters.
 *
 * @return {!Array.<!Object>}
 */
i18n.input.chrome.inputview.content.compact.more.keyPinyinMoreCharacters =
    function() {
  var data = i18n.input.chrome.inputview.content.compact.more.
      keyNAMoreCharacters();
  data[0]['text'] = '\uff5e';
  data[1]['text'] = '\u2194';
  data[1]['moreKeys'] = ['\u2191', '\u2193', '\u2190', '\u2192'];
  data[11]['text'] = '\u00a5';
  data[11]['moreKeys'] = ['\u00a2', '\u00a3', '\u20ac'];
  data[12]['text'] = '\u3010';
  data[13]['text'] = '\u3011';
  data[14]['text'] = '\u300e';
  data[15]['text'] = '\u300f';
  data[15]['moreKeys'] = undefined;
  data[17]['text'] = '\u2026';
  data[17]['moreKeys'] = undefined;
  data[18]['text'] = '\uff5b';
  data[19]['text'] = '\uff5d';
  data[22]['text'] = '\u003d';
  data[22]['moreKeys'] = ['\u2260', '\u2248', '\u221E'];
  data[27]['text'] = '\uff3b';
  data[28]['text'] = '\uff3d';
  data[29]['text'] = '\u300a';
  data[29]['moreKeys'] = ['\u2039', '\u2264', '\u00AB'];
  data[30]['text'] = '\u300b';
  data[30]['moreKeys'] = ['\u203A', '\u2265', '\u00BB'];
  data[33]['text'] = '\uff01';
  data[33]['moreKeys'] = undefined;
  data[35]['text'] = '\uff1f';
  data[35]['moreKeys'] = undefined;
  data[37]['text'] = '\uff0c';
  data[38]['text'] = '\u3002';
  return data;
};

});  // goog.scope
