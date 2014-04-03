// Verifies that the given "bytes" holds the same value as "expectedHexString".
// "bytes" can be anything recognized by "bytesToHexString()".
function bytesShouldMatchHexString(testDescription, expectedHexString, bytes)
{
    expectedHexString = "[" + expectedHexString.toLowerCase() + "]";
    var actualHexString = "[" + bytesToHexString(bytes) + "]";

    if (actualHexString === expectedHexString) {
        debug("PASS: " + testDescription + " should be " + expectedHexString + " and was");
    } else {
        debug("FAIL: " + testDescription + " should be " + expectedHexString + " but was " + actualHexString);
    }
}

// Builds a hex string representation for an array-like input.
// "bytes" can be an Array of bytes, an ArrayBuffer, or any TypedArray.
// The output looks like this:
//    ab034c99
function bytesToHexString(bytes)
{
    if (!bytes)
        return null;

    bytes = new Uint8Array(bytes);
    var hexBytes = [];

    for (var i = 0; i < bytes.length; ++i) {
        var byteString = bytes[i].toString(16);
        if (byteString.length < 2)
            byteString = "0" + byteString;
        hexBytes.push(byteString);
    }

    return hexBytes.join("");
}

function hexStringToUint8Array(hexString)
{
    if (hexString.length % 2 != 0)
        throw "Invalid hexString";
    var arrayBuffer = new Uint8Array(hexString.length / 2);

    for (var i = 0; i < hexString.length; i += 2) {
        var byteValue = parseInt(hexString.substr(i, 2), 16);
        if (byteValue == NaN)
            throw "Invalid hexString";
        arrayBuffer[i/2] = byteValue;
    }

    return arrayBuffer;
}

function asciiToUint8Array(str)
{
    var chars = [];
    for (var i = 0; i < str.length; ++i)
        chars.push(str.charCodeAt(i));
    return new Uint8Array(chars);
}

function failAndFinishJSTest(error)
{
    testFailed('' + error);
    finishJSTest();
}

// Returns a Promise for the cloned key.
function cloneKey(key)
{
    // Sending an object through a MessagePort implicitly clones it.
    // Use a single MessageChannel so requests complete in FIFO order.
    var self = cloneKey;
    if (!self.channel) {
        self.channel = new MessageChannel();
        self.callbacks = [];
        self.channel.port1.addEventListener('message', function(e) {
            var callback = self.callbacks.shift();
            callback(e.data);
        }, false);
        self.channel.port1.start();
    }

    return new Promise(function(resolve, reject) {
        self.callbacks.push(resolve);
        self.channel.port2.postMessage(key);
    });
}

// Logging the serialized format ensures that if it changes it will break tests.
function logSerializedKey(o)
{
    if (internals)
        debug("Serialized key bytes: " + bytesToHexString(internals.serializeObject(o)));
}

function shouldEvaluateAs(actual, expectedValue)
{
    if (typeof expectedValue == "string")
        return shouldBeEqualToString(actual, expectedValue);
    return shouldEvaluateTo(actual, expectedValue);
}
