// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function detectedLanguageSetOnFirst() {
    var first = rootNode.children[0].children[0];
    assertEq('staticText', first.role);
    assertEq('fr', first.language, 'document manually declares lang="fr"');
    assertEq('en', first.detectedLanguage,
      'detected language should be English');
    chrome.test.succeed();
  },

  function detectedLanguageSetOnSecond() {
    var second = rootNode.children[1].children[0];
    assertEq('staticText', second.role);
    assertEq('en', second.language, 'document manually declares lang="en"');
    assertEq('fr', second.detectedLanguage,
      'detected language should be French');
    chrome.test.succeed();
  }
];

setUpAndRunTests(allTests, 'detected_language.html');
