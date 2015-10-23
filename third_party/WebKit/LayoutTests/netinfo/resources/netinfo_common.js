window.jsTestIsAsync = true;

var connection = navigator.connection;
var initialType = "bluetooth";
var initialDownlinkMax = 1.0;
var newConnectionType = "ethernet";
var newDownlinkMax = 2.0;

// Suppress connection messages information from the host.
if (window.internals) {
    internals.setNetworkStateNotifierTestOnly(true);
    internals.setNetworkConnectionInfo(initialType, initialDownlinkMax);

    // Reset the state of the singleton network state notifier.
    window.addEventListener('beforeunload', function() {
        internals.setNetworkStateNotifierTestOnly(false);
    }, false);
}

