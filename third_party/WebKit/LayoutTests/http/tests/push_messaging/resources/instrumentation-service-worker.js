// Allows a document to exercise the Push API within a service worker by sending commands.
var messagePort = null;

self.addEventListener('message', function(workerEvent) {
    messagePort = workerEvent.data;

    // Listen to incoming commands on the message port.
    messagePort.onmessage = function(event) {
        if (typeof event.data != 'object' || !event.data.command)
            return;

        switch (event.data.command) {
            case 'hasPermission':
                self.registration.pushManager.hasPermission().then(function(permissionStatus) {
                    messagePort.postMessage({ command: event.data.command,
                                              success: true,
                                              permission: permissionStatus });
                }, function(error) {
                    messagePort.postMessage({ command: event.data.command,
                                              success: false,
                                              message: error.message });
                });
                break;

            default:
                messagePort.postMessage({ command: 'error',
                                          message: 'Invalid command: ' + event.data.command });
                break;
        }
    };

    // Notify the controller that the worker is now available.
    messagePort.postMessage('ready');
});
