var console = null;

function consoleWrite(text)
{
    if (!console && document.body) {
        console = document.createElement('div');
        document.body.appendChild(console);
    }
    var span = document.createElement('span');
    span.appendChild(document.createTextNode(text));
    span.appendChild(document.createElement('br'));
    console.appendChild(span);
}

// FIXME: Detect EME support rather than just general container support.
// http://crbug.com/441585
// For now, assume that implementations that support a container type for clear
// content and are running these tests also support that container with EME.
// The element used for this will is not released to avoid interfering with the
// ActiveDOMObject counts in the lifetime tests.
var canPlayTypeElement = new Audio();
var isWebMSupported = ('' != canPlayTypeElement.canPlayType('video/webm'));
var isCencSupported = ('' != canPlayTypeElement.canPlayType('video/mp4'));

function isInitDataTypeSupported(initDataType)
{
    var result = false;
    switch (initDataType) {
    case 'webm':
        result = isWebMSupported;
        break;
    case 'cenc':
        result = isCencSupported;
        break;
    default:
        result = false;
    }

    return result;
}


function getInitDataType()
{
    if (isInitDataTypeSupported('webm'))
        return 'webm';
    if (isInitDataTypeSupported('cenc'))
        return 'cenc';
    throw 'No supported Initialization Data Types';
}

function getInitData(initDataType)
{
    // FIXME: This should be dependent on initDataType.
    return new Uint8Array([ 0, 1, 2, 3, 4, 5, 6, 7 ]);
}

function waitForEventAndRunStep(eventName, element, func, stepTest)
{
    var eventCallback = function(event) {
        if (func)
            func(event);
    }
    if (stepTest)
        eventCallback = stepTest.step_func(eventCallback);

    element.addEventListener(eventName, eventCallback, true);
}

// Copied from LayoutTests/resources/js-test.js.
// See it for details of why this is necessary.
function asyncGC(callback)
{
    GCController.collectAll();
    setTimeout(callback, 0);
}

function createGCPromise()
{
    // Run gc() as a promise.
    return new Promise(
        function(resolve, reject) {
            asyncGC(resolve);
        });
}

function delayToAllowEventProcessingPromise()
{
    return new Promise(
        function(resolve, reject) {
            setTimeout(resolve, 0);
        });
}

function stringToUint8Array(str)
{
    var result = new Uint8Array(str.length);
    for(var i = 0; i < str.length; i++) {
        result[i] = str.charCodeAt(i);
    }
    return result;
}

function arrayBufferAsString(buffer)
{
    // MediaKeySession.keyStatuses iterators return an ArrayBuffer,
    // so convert it into a printable string.
    return String.fromCharCode.apply(null, new Uint8Array(buffer));
}

function dumpKeyStatuses(keyStatuses)
{
    consoleWrite("for (var entry of keyStatuses)");
    for (var entry of keyStatuses) {
        consoleWrite(arrayBufferAsString(entry[0]) + ", " + entry[1]);
    }
    consoleWrite("for (var key of keyStatuses.keys())");
    for (var key of keyStatuses.keys()) {
        consoleWrite(arrayBufferAsString(key));
    }
    consoleWrite("for (var value of keyStatuses.values())");
    for (var value of keyStatuses.values()) {
        consoleWrite(value);
    }
    consoleWrite("for (var entry of keyStatuses.entries())");
    for (var entry of keyStatuses.entries()) {
        consoleWrite(arrayBufferAsString(entry[0]) + ", " + entry[1]);
    }
    consoleWrite("keyStatuses.forEach()");
    keyStatuses.forEach(function(value, key, map) {
        consoleWrite(arrayBufferAsString(key) + ", " + value);
    });
}

// Verify that |keyStatuses| contains just the keys in |keys.expected|
// and none of the keys in |keys.unexpected|. All keys should have status
// 'usable'. Example call: verifyKeyStatuses(mediaKeySession.keyStatuses,
// { expected: [key1], unexpected: [key2] });
function verifyKeyStatuses(keyStatuses, keys)
{
    var expected = keys.expected || [];
    var unexpected = keys.unexpected || [];

    // |keyStatuses| should have same size as number of |keys.expected|.
    assert_equals(keyStatuses.size, expected.length);

    // All |keys.expected| should be found.
    expected.map(function(key) {
        assert_true(keyStatuses.has(key));
        assert_equals(keyStatuses.get(key), 'usable');
    });

    // All |keys.unexpected| should not be found.
    unexpected.map(function(key) {
        assert_false(keyStatuses.has(key));
        assert_equals(keyStatuses.get(key), undefined);
    });
}

// Encodes |data| into base64url string. There is no '=' padding, and the
// characters '-' and '_' must be used instead of '+' and '/', respectively.
function base64urlEncode(data)
{
    var result = btoa(String.fromCharCode.apply(null, data));
    return result.replace(/=+$/g, '').replace(/\+/g, "-").replace(/\//g, "_");
}

// Decode |encoded| using base64url decoding.
function base64urlDecode(encoded)
{
    return atob(encoded.replace(/\-/g, "+").replace(/\_/g, "/"));
}

// For Clear Key, the License Format is a JSON Web Key (JWK) Set, which contains
// a set of cryptographic keys represented by JSON. These helper functions help
// wrap raw keys into a JWK set.
// See:
// https://w3c.github.io/encrypted-media/#clear-key-license-format
// http://tools.ietf.org/html/draft-ietf-jose-json-web-key
//
// Creates a JWK from raw key ID and key.
// |keyId| and |key| are expected to be ArrayBufferViews, not base64-encoded.
function createJWK(keyId, key)
{
    var jwk = '{"kty":"oct","alg":"A128KW","kid":"';
    // FIXME: Should use base64URLEncoding.
    jwk += base64urlEncode(keyId);
    jwk += '","k":"';
    jwk += base64urlEncode(key);
    jwk += '"}';
    return jwk;
}

// Creates a JWK Set from multiple JWKs.
function createJWKSet()
{
    var jwkSet = '{"keys":[';
    for (var i = 0; i < arguments.length; i++) {
        if (i != 0)
            jwkSet += ',';
        jwkSet += arguments[i];
    }
    jwkSet += ']}';
    return jwkSet;
}

function forceTestFailureFromPromise(test, error, message)
{
    // Promises convert exceptions into rejected Promises. Since there is
    // currently no way to report a failed test in the test harness, errors
    // are reported using force_timeout().
    if (message)
        consoleWrite(message + ': ' + error.message);
    else if (error)
        consoleWrite(error.message);

    test.force_timeout();
    test.done();
}

function extractSingleKeyIdFromMessage(message)
{
    try {
        var json = JSON.parse(String.fromCharCode.apply(null, new Uint8Array(message)));
        // Decode the first element of 'kids'.
        // FIXME: Switch to base64url. See
        // https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#using-base64url
        assert_equals(1, json.kids.length);
        var decoded_key = base64urlDecode(json.kids[0]);
        // Convert to an Uint8Array and return it.
        return stringToUint8Array(decoded_key);
    }
    catch (o) {
        // Not valid JSON, so return message untouched as Uint8Array.
        // This is for backwards compatibility.
        // FIXME: Remove this once the code is switched to return JSON all
        // the time.
        return new Uint8Array(message);
    }
}
