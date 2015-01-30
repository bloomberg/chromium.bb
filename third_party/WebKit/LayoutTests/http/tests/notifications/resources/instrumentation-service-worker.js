// Allows the controlling document to instrument Web Notification behavior
// within a Service Worker by sending commands.
var messagePort = null;

addEventListener('message', function(workerEvent) {
    messagePort = workerEvent.data;

    // Listen to incoming commands on the message port.
    messagePort.onmessage = function(event) {
        if (typeof event.data != 'object' || !event.data.command)
            return;

        switch (event.data.command) {
            case 'permission':
                messagePort.postMessage({ command: 'permission', value: Notification.permission });
                break;

            case 'show':
                registration.showNotification(event.data.title, event.data.options).then(function() {
                    messagePort.postMessage({ command: 'show', success: true });
                }, function(error) {
                    messagePort.postMessage({ command: 'show', success: false, message: error.message });
                });
                break;

            default:
                messagePort.postMessage({ command: 'error', message: 'Invalid command: ' + event.data.command });
                break;
        }
    };

    // Notify the controller that the worker is now available.
    messagePort.postMessage('ready');
});

addEventListener('notificationclick', function(event) {
    messagePort.postMessage({ command: 'click', notification: {
        title: event.notification.title
    }});

    // Notifications containing "ACTION:CLOSE" in their message will be closed
    // immediately by the Service Worker.
    if (event.notification.body.indexOf('ACTION:CLOSE') != -1)
        event.notification.close();
});
