window.jsTestIsAsync = true;

var connection = navigator.connection;
var initialType = "bluetooth";
var initialDownlinkMax = 1.0;
var newConnectionType = "ethernet";
var newDownlinkMax = 2.0;
var initialEffectiveType = "3g";
var initialRtt = 25.0;
var initialDownlink = 10.0;
var newEffectiveType = "4g";
var newRtt = 50.0;
var newDownlink = 30.0;

// Suppress connection messages information from the host.
if (window.internals) {
    internals.setNetworkConnectionInfoOverride(true, initialType, initialDownlinkMax);
    internals.setNetworkQualityInfoOverride(initialEffectiveType, initialRtt, initialDownlink);

    // Reset the state of the singleton network state notifier.
    window.addEventListener('beforeunload', function() {
        internals.clearNetworkConnectionInfoOverride();
    }, false);
}

function isTypeOnline(type) {
    return type != 'none';
}
