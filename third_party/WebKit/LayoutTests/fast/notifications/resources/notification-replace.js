function runTest()
{
    var notification = new Notification("Notification", {
        tag: "my-notification"
    });

    notification.addEventListener("show", function() {
        testPassed("notification.onshow() has been called.");
        var updatedNotification = new Notification("Notification 2", {
            tag: "my-notification"
        });

        updatedNotification.addEventListener("show", function() {
            testPassed("updatedNotification.onshow() has been called.");
            if (window.testRunner)
                testRunner.notifyDone();
        });
    });
}
