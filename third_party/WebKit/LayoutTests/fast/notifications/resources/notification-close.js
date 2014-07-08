function runTest()
{
    var notification = new Notification("Notification");
    notification.addEventListener("show", function() {
        testPassed("notification.onshow() has been called.");
        notification.close();
    });

    notification.addEventListener("close", function() {
        testPassed("notification.onclose() has been called.");
        if (window.testRunner)
            testRunner.notifyDone();
    });
}
