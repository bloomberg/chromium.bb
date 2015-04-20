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
goog.require('i18n.input.chrome.inputview.content.compact.letter');
goog.require('i18n.input.chrome.inputview.content.compact.more');
goog.require('i18n.input.chrome.inputview.content.compact.symbol');
goog.require('i18n.input.chrome.inputview.content.compact.util');
goog.require('i18n.input.chrome.inputview.content.compact.util.CompactKeysetSpec');
goog.require('i18n.input.chrome.inputview.content.util');
goog.require('i18n.input.chrome.message.ContextType');

(function() {
  var keysetSpecNode =
      i18n.input.chrome.inputview.content.compact.util.CompactKeysetSpec;
  var letter =
      i18n.input.chrome.inputview.content.compact.letter;
  var symbol =
      i18n.input.chrome.inputview.content.compact.symbol;
  var more =
      i18n.input.chrome.inputview.content.compact.more;


  var viewIdPrefix_ = '101kbd-k-';
  var ContextType = i18n.input.chrome.message.ContextType;

  var keyCharacters = [
    ['\u0060', '\u007e'], // TLDE
    ['\u3105', '\u0021'], // AE01
    ['\u3109', '\u0040'], // AE02
    ['\u02c7', '\u0023'], // AE03
    ['\u02cb', '\u0024'], // AE04
    ['\u3113', '\u0025'], // AE05
    ['\u02ca', '\u005e'], // AE06
    ['\u02d9', '\u0026'], // AE07
    ['\u311a', '\u002a'], // AE08
    ['\u311e', '\u0028'], // AE09
    ['\u3122', '\u0029'], // AE10
    ['\u002d', '\u005f'], // AE11
    ['\u003d', '\u002b'], // AE12

    ['\u3106', '\u0051'], // AD01
    ['\u310a', '\u0057'], // AD02
    ['\u310d', '\u0045'], // AD03
    ['\u3110', '\u0052'], // AD04
    ['\u3114', '\u0054'], // AD05
    ['\u3117', '\u0059'], // AD06
    ['\u3127', '\u0055'], // AD07
    ['\u311b', '\u0049'], // AD08
    ['\u311f', '\u004f'], // AD09
    ['\u3123', '\u0050'], // AD10
    ['\u005b', '\u007b'], // AD11
    ['\u005d', '\u007d'], // AD12
    ['\u005c', '\u007c'], // BKSL

    ['\u3107', '\u0041'], // AC01
    ['\u310B', '\u0053'], // AC02
    ['\u310e', '\u0044'], // AC03
    ['\u3111', '\u0046'], // AC04
    ['\u3115', '\u0047'], // AC05
    ['\u3118', '\u0048'], // AC06
    ['\u3128', '\u004a'], // AC07
    ['\u311c', '\u004b'], // AC08
    ['\u3120', '\u004c'], // AC09
    ['\u3124', '\u003a'], // AC10
    ['\u0027', '\u0022'], // AC11

    ['\u3108', '\u005a'], // AB01
    ['\u310c', '\u0058'], // AB02
    ['\u310f', '\u0043'], // AB03
    ['\u3112', '\u0056'], // AB04
    ['\u3116', '\u0042'], // AB05
    ['\u3119', '\u004e'], // AB06
    ['\u3129', '\u004d'], // AB07
    ['\u311d', '\u003c'], // AB08
    ['\u3121', '\u003e'], // AB09
    ['\u3125', '\u003f'], // AB10
    ['\u0020', '\u0020'] // SPCE
  ];




  var letterKeysetSpec = {};
  letterKeysetSpec[keysetSpecNode.ID] = 'zhuyin.compact.qwerty';
  letterKeysetSpec[keysetSpecNode.LAYOUT] = 'compactkbd-zhuyin';
  letterKeysetSpec[keysetSpecNode.DATA] = letter.keyZhuyinCharacters();

  var enKeysetSpecNode =
      i18n.input.chrome.inputview.content.compact.util.CompactKeysetSpec;
  var enLetterKeysetSpec = {};
  enLetterKeysetSpec[enKeysetSpecNode.ID] = 'zhuyin.en.compact.qwerty';
  enLetterKeysetSpec[enKeysetSpecNode.LAYOUT] = 'compactkbd-qwerty';
  enLetterKeysetSpec[enKeysetSpecNode.DATA] = letter.keyEnCharacters();

  var symbolKeysetSpec = {};
  symbolKeysetSpec[keysetSpecNode.ID] = 'zhuyin.compact.symbol';
  symbolKeysetSpec[keysetSpecNode.LAYOUT] = 'compactkbd-qwerty';
  symbolKeysetSpec[keysetSpecNode.DATA] = symbol.keyPinyinSymbolCharacters();

  var enSymbolKeysetSpec = {};
  enSymbolKeysetSpec[keysetSpecNode.ID] = 'zhuyin.en.compact.symbol';
  enSymbolKeysetSpec[keysetSpecNode.LAYOUT] = 'compactkbd-qwerty';
  enSymbolKeysetSpec[keysetSpecNode.DATA] = symbol.keyNASymbolCharacters();

  var moreKeysetSpec = {};
  moreKeysetSpec[keysetSpecNode.ID] = 'zhuyin.compact.more';
  moreKeysetSpec[keysetSpecNode.LAYOUT] = 'compactkbd-qwerty';
  moreKeysetSpec[keysetSpecNode.DATA] = more.keyPinyinMoreCharacters();

  var enMoreKeysetSpec = {};
  enMoreKeysetSpec[keysetSpecNode.ID] = 'zhuyin.en.compact.more';
  enMoreKeysetSpec[keysetSpecNode.LAYOUT] = 'compactkbd-qwerty';
  enMoreKeysetSpec[keysetSpecNode.DATA] = more.keyNAMoreCharacters();

  i18n.input.chrome.inputview.content.compact.util.
      generatePinyinCompactKeyboard(
      letterKeysetSpec, enLetterKeysetSpec, symbolKeysetSpec,
      enSymbolKeysetSpec, moreKeysetSpec, enMoreKeysetSpec,
      google.ime.chrome.inputview.onConfigLoaded);

  var data = i18n.input.chrome.inputview.content.util.createData(
      keyCharacters, viewIdPrefix_, false, false, undefined,
      'zhuyin.compact.qwerty');
  data['id'] = 'zhuyin';
  google.ime.chrome.inputview.onConfigLoaded(data);
}) ();
