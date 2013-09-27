function importTestKeys()
{
    var keyFormat = "raw";
    var data = asciiToArrayBuffer("16 bytes of key!");
    var extractable = true;
    var keyUsages = ['encrypt', 'decrypt', 'sign', 'verify'];

    var hmacPromise = crypto.subtle.importKey(keyFormat, data, {name: 'hmac', hash: {name: 'sha-1'}}, extractable, keyUsages);
    var aesCbcPromise = crypto.subtle.importKey(keyFormat, data, {name: 'AES-CBC'}, extractable, keyUsages);
    var aesCbcJustDecrypt = crypto.subtle.importKey(keyFormat, data, {name: 'AES-CBC'}, false, ['decrypt']);

    return Promise.all([hmacPromise, aesCbcPromise, aesCbcJustDecrypt]).then(function(results) {
        return {
            hmacSha1: results[0],
            aesCbc: results[1],
            aesCbcJustDecrypt: results[2],
        };
    });
}

// Builds a hex string representation of any array-like input (array or
// ArrayBufferView). The output looks like this:
//    [ab 03 4c 99]
function byteArrayToHexString(bytes)
{
    var hexBytes = [];

    for (var i = 0; i < bytes.length; ++i) {
        var byteString = bytes[i].toString(16);
        if (byteString.length < 2)
            byteString = "0" + byteString;
        hexBytes.push(byteString);
    }

    return "[" + hexBytes.join(" ") + "]";
}

function arrayBufferToHexString(buffer)
{
    return byteArrayToHexString(new Uint8Array(buffer));
}

function asciiToArrayBuffer(str)
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
