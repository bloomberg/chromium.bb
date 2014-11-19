// Supports test-runner control messages being send over |messagePort|, which enable
// workers to have limited access to TestRunner methods.
function supportTestRunnerMessagesOnPort(messagePort)
{
    if (!window.testRunner)
        return;

    messagePort.addEventListener('message', function(message) {
        if (message.data.type == 'simulateWebNotificationClick')
            testRunner.simulateWebNotificationClick(message.data.title);
    });
}

// Starts |script| as a dedicated worker and executes the testharness tests defined
// within it. The Notification-related testRunner methods will be made available.
function dedicatedWorkerTest(script)
{
    var worker = new Worker(script);
    supportTestRunnerMessagesOnPort(worker);

    fetch_tests_from_worker(worker);
}

// Starts |script| as a shared worker and executes the testharness tests defined
// within it. The Notification-related testRunner methods will be made available.
function sharedWorkerTest(script)
{
    var worker = new SharedWorker(script, 'Notification API LayoutTest worker');
    supportTestRunnerMessagesOnPort(worker.port);

    fetch_tests_from_worker(worker);
}

// Used by tests to announce that all tests have been registered when they're
// being ran from a dedicated or shared worker. Not necessary for document tests.
function isDedicatedOrSharedWorker()
{
    return false;
}