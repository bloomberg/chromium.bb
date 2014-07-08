function runTest()
{
    var notification = new Notification("Notification");
    notification.addEventListener("show", function() {
        testPassed("notification.onshow() has been called.");
        if (window.testRunner)
            testRunner.simulateWebNotificationClick("Notification");
    });

    notification.addEventListener("click", function() {
        testPassed("notification.onclick() has been called.");
        notification.close();

        if (window.testRunner)
            testRunner.notifyDone();
    });
}
