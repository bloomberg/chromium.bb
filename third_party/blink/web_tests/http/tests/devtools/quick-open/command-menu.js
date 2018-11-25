// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that the command menu is properly filled.\n`);


  self.runtime.loadModulePromise('quick_open').then(() => {
    var categories = new Set();
    var commands = new Map();
    QuickOpen.commandMenu.commands().forEach(command => {
      categories.add(command.category());
      commands.set(command.category() + ': ' + command.title(), command);
    });

    TestRunner.addResult('Categories active:');
    Array.from(categories).sort().forEach(category => TestRunner.addResult('Has category: ' + category));

    TestRunner.addResult('');
    var whitelist = [
      'Panel: Show Console', 'Drawer: Show Console', 'Appearance: Switch to dark theme',
      'DevTools: Auto-open DevTools for popups'
    ];
    whitelist.forEach(item => {
      if (!commands.has(item))
        TestRunner.addResult(item + ' is MISSING');
    });

    TestRunner.addResult('Switching to console panel');
    try {
      commands.get('Panel: Show Console')._executeHandler().then(() => {
        TestRunner.addResult('Current panel: ' + UI.inspectorView.currentPanelDeprecated().name);
        TestRunner.completeTest();
      });
    } catch (e) {
      TestRunner.addResult(e);
      TestRunner.completeTest();
    }
  });
})();
