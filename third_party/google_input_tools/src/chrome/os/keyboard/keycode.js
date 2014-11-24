// Copyright 2013 The ChromeOS IME Authors. All Rights Reserved.
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

/**
 * @fileoverview Defines all the key codes and key codes map.
 */

goog.provide('i18n.input.chrome.vk.KeyCode');


/**
 * The standard 101 keyboard keys. Each char represent the key code of each key.
 *
 * @type {string}
 */
i18n.input.chrome.vk.KeyCode.CODES101 =
    '\u00c01234567890\u00bd\u00bb' +
    'QWERTYUIOP\u00db\u00dd\u00dc' +
    'ASDFGHJKL\u00ba\u00de' +
    'ZXCVBNM\u00bc\u00be\u00bf' +
    '\u0020';


/**
 * The standard 102 keyboard keys.
 *
 * @type {string}
 */
i18n.input.chrome.vk.KeyCode.CODES102 =
    '\u00c01234567890\u00bd\u00bb' +
    'QWERTYUIOP\u00db\u00dd' +
    'ASDFGHJKL\u00ba\u00de\u00dc' +
    '\u00e2ZXCVBNM\u00bc\u00be\u00bf' +
    '\u0020';


/**
 * The standard 101 keyboard keys, including the BS/TAB/CAPS/ENTER/SHIFT/ALTGR.
 *     Each char represent the key code of each key.
 *
 * @type {string}
 */
i18n.input.chrome.vk.KeyCode.ALLCODES101 =
    '\u00c01234567890\u00bd\u00bb\u0008' +
    '\u0009QWERTYUIOP\u00db\u00dd\u00dc' +
    '\u0014ASDFGHJKL\u00ba\u00de\u000d' +
    '\u0010ZXCVBNM\u00bc\u00be\u00bf\u0010' +
    '\u0111\u0020\u0111';


/**
 * The standard 102 keyboard keys, including the BS/TAB/CAPS/ENTER/SHIFT/ALTGR.
 *     Each char represent the key code of each key.
 *
 * @type {string}
 */
i18n.input.chrome.vk.KeyCode.ALLCODES102 =
    '\u00c01234567890\u00bd\u00bb\u0008' +
    '\u0009QWERTYUIOP\u00db\u00dd\u000d' +
    '\u0014ASDFGHJKL\u00ba\u00de\u00dc\u000d' +
    '\u0010\u00e2ZXCVBNM\u00bc\u00be\u00bf\u0010' +
    '\u0111\u0020\u0111';
