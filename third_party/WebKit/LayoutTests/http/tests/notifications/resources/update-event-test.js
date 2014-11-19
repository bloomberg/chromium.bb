async_test(function(test) {
    if (Notification.permission != 'granted') {
        assert_unreached('No permission has been granted for displaying notifications.');
        return;
    }

    var notification = new Notification('My Notification', { tag: 'notification-test' });
    notification.addEventListener('show', function() {
        var updatedNotification = new Notification('Second Notification', { tag: 'notification-test' });
        updatedNotification.addEventListener('show', function() {
            test.done();
        });
    });

    // FIXME: Replacing a notification should fire the close event on the
    // notification that's being replaced.

    notification.addEventListener('error', function() {
        assert_unreached('The error event should not be thrown.');
    });

}, 'Replacing a notification will discard the previous notification.');
