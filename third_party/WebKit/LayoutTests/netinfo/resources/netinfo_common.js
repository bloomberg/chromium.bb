window.jsTestIsAsync = true;

// Suppress connection messages information from the host.
if (window.internals) {
    internals.setNetworkStateNotifierTestOnly(true);
}

var connection = navigator.connection;
var initialType = connection.type;
var newConnectionType = connection.type == "bluetooth" ? "ethernet" : "bluetooth";