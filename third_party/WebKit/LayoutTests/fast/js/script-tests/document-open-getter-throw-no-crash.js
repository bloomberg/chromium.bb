description("document.open() - propagate exception thrown when accessing window.open.");

var frame = document.body.appendChild(document.createElement("iframe"));
frame.contentWindow.__defineGetter__("open", function() { throw 'PASS (no crash)';});
shouldThrow("frame.contentDocument.open(1, 1, 1, 1, 1);");

