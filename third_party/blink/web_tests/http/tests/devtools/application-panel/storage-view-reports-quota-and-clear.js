// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests quota reporting.\n`);
  await TestRunner.loadModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('resources');

  var updateListener = null;

  async function writeToIndexdDB() {
    var array = [];
    for (var i = 0; i < 20000; i++)
      array.push(i % 10);
    var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
    await new Promise(resolve => ApplicationTestRunner.createDatabase(mainFrameId, 'Database1', resolve));
    await new Promise(
        resolve => ApplicationTestRunner.createObjectStore(mainFrameId, 'Database1', 'Store1', 'id', true, resolve));
    await new Promise(
        resolve =>
            ApplicationTestRunner.addIDBValue(mainFrameId, 'Database1', 'Store1', {key: 1, value: array}, '', resolve));
  }

  async function dumpWhenMatches(view, predicate) {
    await new Promise(resolve => {
      function sniffer(usage, quota) {
        if (usage !== null && (!predicate || predicate(usage, quota)))
          resolve();
        else
          TestRunner.addSniffer(clearStorageView, '_usageUpdatedForTest', sniffer);
      }
      sniffer(null);
    });
    // Quota will vary between setups, rather strip it altogether
    var clean = view._quotaRow.innerHTML.replace(/\&nbsp;/g, ' ');
    // Clean usage value because it's platform-dependent.
    var quotaStripped = clean.replace(/[\d.]+ (K?B used out of) \d+ .?B([^\d]*)/, '-- $1 --$2');
    TestRunner.addResult(quotaStripped);

    TestRunner.addResult('Usage breakdown:');
    for (var i = 0; i < view._pieChartLegend.children.length; i++) {
      var typeUsage = ': ';
      var children = view._pieChartLegend.children[i].children;
      for (var j = 0; j < children.length; j++) {
        if (children[j].classList.contains('usage-breakdown-legend-title'))
          typeUsage = children[j].textContent + typeUsage;
        if (children[j].classList.contains('usage-breakdown-legend-value')) {
          // Clean usage value because it's platform-dependent.
          var cleanedValue = children[j].textContent.replace(/\d+(.\d+)?\sKB/, '--.- KB')
                                                    .replace(/\d+(.\d+)?\sB/, '--.- B');
          typeUsage = typeUsage + cleanedValue;
        }
      }
      TestRunner.addResult(typeUsage);
    }
  }

  function isServiceWorkerStopping(registration) {
    const version = registration.versionsByMode().get(SDK.ServiceWorkerVersion.Modes.Redundant);
    if (!version)
      return null;
    const status = version ? version.runningStatus : null;
    return status === 'stopping';
  }

  UI.viewManager.showView('resources');

  var parent = UI.panels.resources._sidebar._applicationTreeElement;
  var clearStorageElement = parent.children().find(child => child.title === 'Clear storage');

  TestRunner.addResult('Tree element found: ' + !!clearStorageElement);
  clearStorageElement.select();

  var clearStorageView = UI.panels.resources.visibleView;
  TestRunner.addResult('Clear storage view is visible: ' + (clearStorageView instanceof Resources.ClearStorageView));

  TestRunner.markStep('Clear all data');

  clearStorageView._clearButton.click();
  await dumpWhenMatches(clearStorageView, usage => usage === 0);

  TestRunner.markStep('Now with data');

  await writeToIndexdDB();

  var scriptURL = 'http://127.0.0.1:8000/devtools/service-workers/resources/service-worker-empty.js';
  var scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/scope/';
  await ApplicationTestRunner.registerServiceWorker(scriptURL, scope);
  await dumpWhenMatches(clearStorageView, usage => usage > 20000);

  TestRunner.markStep('Clear all data, again');

  for (const serviceWorkerManager of SDK.targetManager.models(SDK.ServiceWorkerManager)) {
    serviceWorkerManager.addEventListener(
    SDK.ServiceWorkerManager.Events.RegistrationUpdated, (event) => {
      if (isServiceWorkerStopping(event.data))
        TestRunner.addResult('service worker is stopping');
      }, this);
  }

  clearStorageView._clearButton.click();

  await dumpWhenMatches(clearStorageView, usage => usage === 0);

  TestRunner.completeTest();
})();
