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

function getInitDataType()
{
    return (MediaKeys.isTypeSupported('org.w3.clearkey', 'video/webm')) ? 'video/webm' : 'video/mp4';
}

function getInitData(initDataType)
{
    // FIXME: This should be dependent on initDataType.
    return new Uint8Array([ 0, 1, 2, 3, 4, 5, 6, 7 ]);
}

function waitForEventAndRunStep(eventName, element, func, stepTest)
{
    var eventCallback = function(event) {
        consoleWrite('EVENT(' + eventName + ')');
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

// For Clear Key, MediaKeySession.update() takes a JSON Web Key (JWK) Set,
// which contains a set of cryptographic keys represented by JSON. These helper
// functions help wrap raw keys into a JWK set.
// See:
// https://dvcs.w3.org/hg/html-media/raw-file/tip/encrypted-media/encrypted-media.html#simple-decryption-clear-key
// http://tools.ietf.org/html/draft-ietf-jose-json-web-key

// Encodes data into base64 string without trailing '='.
function base64Encode(data)
{
    var result = btoa(String.fromCharCode.apply(null, data));
    return result.replace(/=+$/g, '');
}

// Creates a JWK from raw key ID and key.
// |keyId| and |key| are expected to be ArrayBufferViews, not base64-encoded.
function createJWK(keyId, key)
{
    var jwk = '{"kty":"oct","kid":"';
    // FIXME: Should use base64URLEncoding.
    jwk += base64Encode(keyId);
    jwk += '","k":"';
    jwk += base64Encode(key);
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
