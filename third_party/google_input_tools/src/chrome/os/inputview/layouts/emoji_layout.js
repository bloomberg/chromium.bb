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
goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.elements.ElementType');
goog.require('i18n.input.chrome.inputview.layouts.util');

(function() {
  var util = i18n.input.chrome.inputview.layouts.util;
  var ElementType = i18n.input.chrome.inputview.elements.ElementType;
  util.setPrefix('emoji-k-');
  var ids = ['recent', 'favorits', 'faces', 'emoticon',
    'symbol', 'nature', 'places', 'objects'];
  var length = [1, 3, 6, 4, 6, 7, 5, 8];
  var emojiPage = {};
  var emojiList = [];
  // Tab Rows
  var tabRows = [];
  var keySpec = {
    'widthInWeight': 1,
    'heightInWeight': 13
  };
  var baseSpec = {
    'widthInWeight': 1.4,
    'heightInWeight': 13
  };

  //TODO: needs modification when there are more kinds of emoji.
  for (var i = 0, len = ids.length; i < len; i++) {
    tabRows.push(util.createKey(keySpec));
  }
  tabRows.push(util.createKey(baseSpec));
  tabRows.push(util.createKey(keySpec));
  var tabBar = util.createLinearLayout({
    'id': 'tabBar',
    'children': tabRows,
    'iconCssClass': i18n.input.chrome.inputview.Css.LINEAR_LAYOUT_BORDER
  });

  keySpec = {
    'widthInWeight': 1,
    'heightInWeight': 19
  };
  for (var i = 0, len = ids.length; i < len; ++i) {
    for (var j = 0, lenJ = length[i]; j < lenJ; ++j) {
      // Row1
      var keysOfRow1 = util.createKeySequence(keySpec, 9);
      var row1 = util.createLinearLayout({
        'id': ids[i] + 'Row1',
        'children': [keysOfRow1]
      });

      // Row2
      var keysOfRow2 = util.createKeySequence(keySpec, 9);
      var row2 = util.createLinearLayout({
        'id': ids[i] + 'Row2',
        'children': [keysOfRow2]
      });

      // Row3
      var keysOfRow3 = util.createKeySequence(keySpec, 9);
      var row3 = util.createLinearLayout({
        'id': ids[i] + 'Row3',
        'children': [keysOfRow3]
      });

      emojiPage = util.createVerticalLayout({
        'id': ids[i] + j,
        'children': [row1, row2, row3]
      });
      emojiList.push(emojiPage);
    }
  }
  var emojiRows = util.createExtendedLayout({
    'id': 'emojiRows',
    'children': emojiList
  });
  keySpec = {
    'widthInWeight': 1,
    'heightInWeight': 1
  };

  var emojiSlider = util.createVerticalLayout({
    'id': 'emojiSlider',
    'children': [emojiRows]
  });

  keySpec = {
    'widthInWeight': 1.42,
    'heightInWeight': 14
  };
  var sideKeys = util.createVerticalLayout({
    'id': 'sideKeys',
    'children': [util.createKey(keySpec), util.createKey(keySpec),
      util.createKey(keySpec)]
  });

  var rowsAndSideKeys = util.createLinearLayout({
    'id': 'rowsAndSideKeys',
    'children': [emojiSlider, sideKeys]
  });
  var emojiView = util.createVerticalLayout({
    'id': 'emojiView',
    'children': [tabBar, rowsAndSideKeys]
  });

  // Keyboard view.
  var keyboardView = util.createLayoutView({
    'id': 'keyboardView',
    'children': [emojiView],
    'widthPercent': 100,
    'heightPercent': 100
  });


  var keyboardContainer = util.createLinearLayout({
    'id': 'keyboardContainer',
    'children': [keyboardView]
  });

  var data = {
    'disableCandidateView': true,
    'layoutID': 'emoji',
    'children': [keyboardContainer]
  };

  google.ime.chrome.inputview.onLayoutLoaded(data);
}) ();
