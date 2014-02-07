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
function createJWK(keyId, key)
{
    var jwk = "{\"kty\":\"oct\",\"kid\":\"";
    jwk += base64Encode(keyId);
    jwk += "\",\"k\":\"";
    jwk += base64Encode(key);
    jwk += "\"}";
    return jwk;
}

// Creates a JWK Set from multiple JWKs.
function createJWKSet()
{
    var jwkSet = "{\"keys\":[";
    for (var i = 0; i < arguments.length; i++) {
        if (i != 0)
            jwkSet += ",";
        jwkSet += arguments[i];
    }
    jwkSet += "]}";
    return jwkSet;
}

