(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that service worker requests are intercepted.`);

  const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
  const globalFetcher = new FetchHelper(testRunner, testRunner.browserP());
  await globalFetcher.enable();

  globalFetcher.onRequest().continueRequest({});

  // TODO(caseq): add tests for target-specific interception.

  await dp.ServiceWorker.enable();
  await session.navigate("resources/service-worker.html");
  session.evaluateAsync(`installServiceWorker()`);

  async function waitForServiceWorkerActivation() {
    let versions;
    do {
      const result = await dp.ServiceWorker.onceWorkerVersionUpdated();
      versions = result.params.versions;
    } while (!versions.length || versions[0].status !== "activated");
    return versions[0].registrationId;
  }

  await waitForServiceWorkerActivation();
  dp.Page.reload();
  await dp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  globalFetcher.onceRequest().fulfill({
    responseCode: 200,
    responseHeaders: [],
    body: btoa("overriden response body")
  });

  const url = 'fetch-data.txt';
  let content = await session.evaluateAsync(`fetch("${url}?fulfill-by-sw").then(r => r.text())`);
  testRunner.log(`Response fulfilled by service worker: ${content}`);
  content = await session.evaluateAsync(`fetch("${url}").then(r => r.text())`);
  testRunner.log(`Response after Fetch.fulfillRequest: ${content}`);
  testRunner.completeTest();
})
