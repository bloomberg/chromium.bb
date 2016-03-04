onmessage = function () {
    postMessage({
        "origin": self.location.origin,
        "addressSpace": self.addressSpace
    });
}
