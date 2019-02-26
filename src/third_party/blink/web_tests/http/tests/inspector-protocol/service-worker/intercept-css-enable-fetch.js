(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/repeat-fetch-service-worker.html',
      'Verifies that service workers do not throw errors from devtools css enable initiated fetch.');

  dp.ServiceWorker.onWorkerErrorReported(error => {
    testRunner.log(
        'serivce worker reported error: ' + JSON.stringify(error, null, 2));
    testRunner.completeTest();
  });

  await dp.Runtime.enable();
  await dp.ServiceWorker.enable();

  let versions;
  do {
    const result = await dp.ServiceWorker.onceWorkerVersionUpdated();
    versions = result.params.versions;
  } while (!versions.length || versions[0].status !== 'activated');
  await versions[0].registrationId;

  await dp.Page.enable();
  await dp.Page.reload();

  await dp.DOM.enable();
  await dp.CSS.enable();

  testRunner.log('finished awaiting for css enable');
  testRunner.completeTest();
});
