function runTest()
{
    var notification = new Notification('Hello, world!');
    notification.onshow = function() {
        debug('notification.onshow fired.');
        if (window.testRunner)
            testRunner.notifyDone();
    };

    notification.onerror = function() {
        debug('notification.onerror fired.');
        if (window.testRunner)
            testRunner.notifyDone();
    };
}
