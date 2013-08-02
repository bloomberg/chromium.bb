function importHmacSha1Key()
{
    var keyFormat = "spki";
    var data = new Uint8Array([]);
    var algorithm = {name: 'hmac', hash: {name: 'sha-1'}};
    var extractable = false;
    var keyUsages = ['encrypt', 'decrypt', 'sign', 'verify'];

    return crypto.subtle.importKey(keyFormat, data, algorithm, extractable, keyUsages);
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

function asciiToArrayBuffer(str)
{
    var chars = [];
    for (var i = 0; i < str.length; ++i)
        chars.push(str.charCodeAt(i));
    return new Uint8Array(chars);
}

