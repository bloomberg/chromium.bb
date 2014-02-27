// The following key pair is comprised of the SPKI (public key) and PKCS#8
// (private key) representations of the key pair provided in Example 1 of the
// NIST test vectors at ftp://ftp.rsa.com/pub/rsalabs/tmp/pkcs1v15sign-vectors.txt
var kPublicKeySpkiDerHex = "30819f300d06092a864886f70d010101050003818d0030818902818100a56e4a0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056ffedb162b4c0f283a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8b6df5d671ef6377c0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b6f64c4ef22e1e1f20d0ce8cffb2249bd9a21370203010001";
var kPrivateKeyPkcs8DerHex = "30820275020100300d06092a864886f70d01010105000482025f3082025b02010002818100a56e4a0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056ffedb162b4c0f283a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8b6df5d671ef6377c0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b6f64c4ef22e1e1f20d0ce8cffb2249bd9a2137020301000102818033a5042a90b27d4f5451" +
                             "ca9bbbd0b44771a101af884340aef9885f2a4bbe92e894a724ac3c568c8f97853ad07c0266c8c6a3ca0929f1e8f11231884429fc4d9ae55fee896a10ce707c3ed7e734e44727a39574501a532683109c2abacaba283c31b4bd2f53c3ee37e352cee34f9e503bd80c0622ad79c6dcee883547c6a3b325024100e7e8942720a877517273a356053ea2a1bc0c94aa72d55c6e86296b2dfc967948c0a72cbccca7eacb35706e09a1df55a1535bd9b3cc34160b3b6dcd3eda8e6443024100b69dca1cf7d4d7ec81e75b90fcca874abcde123fd2700180aa90479b6e48de8d67ed24f9f19d85ba275874f542cd20dc723e6963364a1f9425452b269a6799fd024028fa13938655be1f8a159cbaca5a72ea190c30089e19cd274a556f36c4f6e19f554b34c077790427bbdd8dd3ede2448328f385d81b30e8e43b2fffa02786197902401a8b38f398fa712049898d7fb79ee0a77668791299cdfa09efc0e507acb21ed74301ef5bfd48be455eaeb6e1678255827580a8e4e8e14151d1510a82a3f2e729024027156aba4126d24a81f3a528cbfb27f56886f840a9f6e86e17a44b94fe9319584b8e22fdde1e5a2e3bd8aa5ba8d8584194eb2190acf832b847f13a3d24a79f4d";

function importTestKeys()
{
    var data = asciiToUint8Array("16 bytes of key!");
    var extractable = true;
    var keyUsages = ['wrapKey', 'unwrapKey', 'encrypt', 'decrypt', 'sign', 'verify'];

    var hmacPromise = crypto.subtle.importKey('raw', data, {name: 'hmac', hash: {name: 'sha-1'}}, extractable, keyUsages);
    var aesCbcPromise = crypto.subtle.importKey('raw', data, {name: 'AES-CBC'}, extractable, keyUsages);
    var aesCbcJustDecrypt = crypto.subtle.importKey('raw', data, {name: 'AES-CBC'}, false, ['decrypt']);
    // FIXME: use AES-CTR key type once it's implemented
    var aesCtrPromise = crypto.subtle.importKey('raw', data, {name: 'AES-CBC'}, extractable, keyUsages);
    var aesGcmPromise = crypto.subtle.importKey('raw', data, {name: 'AES-GCM'}, extractable, keyUsages);
    var rsaSsaSha1PublicPromise = crypto.subtle.importKey('spki', hexStringToUint8Array(kPublicKeySpkiDerHex), {name: 'RSASSA-PKCS1-v1_5', hash: {name: 'sha-1'}}, extractable, keyUsages);
    var rsaSsaSha1PrivatePromise = crypto.subtle.importKey('pkcs8', hexStringToUint8Array(kPrivateKeyPkcs8DerHex), {name: 'RSASSA-PKCS1-v1_5', hash: {name: 'sha-1'}}, extractable, keyUsages);

    return Promise.all([hmacPromise, aesCbcPromise, aesCbcJustDecrypt, aesCtrPromise, aesGcmPromise, rsaSsaSha1PublicPromise, rsaSsaSha1PrivatePromise]).then(function(results) {
        return {
            hmacSha1: results[0],
            aesCbc: results[1],
            aesCbcJustDecrypt: results[2],
            aesCtr: results[3],
            aesGcm: results[4],
            rsaSsaSha1Public: results[5],
            rsaSsaSha1Private: results[6],
        };
    });
}

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
    if (error)
       debug(error);
    finishJSTest();
}

numOutstandingTasks = 0;

function addTask(promise)
{
    numOutstandingTasks++;

    function taskFinished()
    {
        numOutstandingTasks--;
        completeTestWhenAllTasksDone();
    }

    promise.then(taskFinished, taskFinished);
}

function completeTestWhenAllTasksDone()
{
    if (numOutstandingTasks == 0) {
        finishJSTest();
    }
}

function shouldRejectPromiseWithNull(code)
{
    var promise = eval(code);

    function acceptCallback(result)
    {
        debug("FAIL: '" + code + "' accepted with " + result + " but should have been rejected");
    }

    function rejectCallback(result)
    {
        if (result == null)
            debug("PASS: '" + code + "' rejected with null");
        else
            debug("FAIL: '" + code + "' rejected with " + result + " but was expecting null");
    }

    addTask(promise.then(acceptCallback, rejectCallback));
}

function shouldAcceptPromise(code)
{
    var promise = eval(code);

    function acceptCallback(result)
    {
        debug("PASS: '" + code + "' accepted with " + result);
    }

    function rejectCallback(result)
    {
        debug("FAIL: '" + code + "' rejected with " + result);
    }

    addTask(promise.then(acceptCallback, rejectCallback));
}
