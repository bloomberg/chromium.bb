'use strict';

// Common mock values for the mockAuthenticator.
var CHALLENGE = new TextEncoder().encode("climb a mountain");

var PUBLIC_KEY_RP = {
    id: "subdomain.example.test",
    name: "Acme"
};

var PUBLIC_KEY_USER = {
    id: new TextEncoder().encode("1098237235409872"),
    name: "avery.a.jones@example.com",
    displayName: "Avery A. Jones",
    icon: "https://pics.acme.com/00/p/aBjjjpqPb.png"
};

var PUBLIC_KEY_PARAMETERS =  [{
    type: "public-key",
    alg: -7,
},];

var AUTHENTICATOR_SELECTION_CRITERIA = {
    requireResidentKey: false,
    userVerification: "preferred",
};

var MAKE_CREDENTIAL_OPTIONS = {
    challenge: CHALLENGE,
    rp: PUBLIC_KEY_RP,
    user: PUBLIC_KEY_USER,
    pubKeyCredParams: PUBLIC_KEY_PARAMETERS,
    authenticatorSelection: AUTHENTICATOR_SELECTION_CRITERIA,
    excludeCredentials: [],
};

var ACCEPTABLE_CREDENTIAL = {
    type: "public-key",
    id: new TextEncoder().encode("acceptableCredential"),
    transports: ["usb", "nfc", "ble"]
};

var GET_CREDENTIAL_OPTIONS = {
    challenge: CHALLENGE,
    rpId: "subdomain.example.test",
    allowCredentials: [ACCEPTABLE_CREDENTIAL],
    userVerification: "preferred",
};

var RAW_ID = new TextEncoder("utf-8").encode("rawId");
var ID = btoa("rawId");
var CLIENT_DATA_JSON = new TextEncoder("utf-8").encode("clientDataJSON");
var ATTESTATION_OBJECT = new TextEncoder("utf-8").encode("attestationObject");
var AUTHENTICATOR_DATA = new TextEncoder("utf-8").encode("authenticatorData");
var SIGNATURE = new TextEncoder("utf-8").encode("signature");

var TEST_NESTED_CREDENTIAL_ID = "nestedCredentialId";

// Use a 3-second timeout in the parameters for "success" cases
// so that each test will exercise the rpID checks in both the renderer
// and browser but will time out instead of wait for a device response.
var CUSTOM_MAKE_CREDENTIAL_OPTIONS = 'var customPublicKey = '
    + '{challenge: new TextEncoder().encode("challenge"), '
    + 'rp: {id: "subdomain.example.test", name: "Acme"}, '
    + 'user: {id: new TextEncoder().encode("1098237235409872"), '
    + 'name: "acme@example.com", displayName: "Acme", icon:"iconUrl"}, '
    + 'timeout: 2000, '
    + 'pubKeyCredParams: [{type: "public-key", alg: -7,},], excludeCredentials:[],};';

var CREATE_CUSTOM_CREDENTIALS = CUSTOM_MAKE_CREDENTIAL_OPTIONS
    + "navigator.credentials.create({publicKey : customPublicKey})"
    + ".then(c => window.parent.postMessage(c.id, '*'))"
    + ".catch(e => window.parent.postMessage(e.name, '*'));";

var CREATE_CREDENTIALS = "navigator.credentials.create({publicKey : MAKE_CREDENTIAL_OPTIONS})"
    + ".then(c => window.parent.postMessage(c.id, '*'));";

var CUSTOM_GET_CREDENTIAL_OPTIONS = 'var customPublicKey = '
    + '{challenge: new TextEncoder().encode("challenge"), '
    + 'rpId: "subdomain.example.test", '
    + 'timeout: 2000, '
    + 'allowCredentials: [{type: "public-key", id: new TextEncoder().encode("allowedCredential"), transports: ["usb", "nfc", "ble"]},],};';

var GET_CUSTOM_CREDENTIALS = CUSTOM_GET_CREDENTIAL_OPTIONS
    + "navigator.credentials.get({publicKey : customPublicKey})"
    + ".then(c => window.parent.postMessage(c.id, '*'))"
    + ".catch(e => window.parent.postMessage(e.name, '*'));";

var GET_CREDENTIAL = "navigator.credentials.get({publicKey : GET_CREDENTIAL_OPTIONS})"
    + ".then(c => window.parent.postMessage(c.id, '*'));";

function EncloseInScriptTag(code) {
  return "<script>" + code + "</scr" + "ipt>";
}

function deepCopy(value) {
  if ([Number, String, Uint8Array].includes(value.constructor))
    return value;

  let copy = (value.constructor == Array) ? [] : {};
  for (let key of Object.keys(value))
    copy[key] = deepCopy(value[key]);
  return copy;
}

// Verifies if |r| is the valid response to credentials.create(publicKey).
function assertValidMakeCredentialResponse(r) {
assert_equals(r.id, ID, 'id');
    assert_true(r.rawId instanceof ArrayBuffer);
    assert_array_equals(new Uint8Array(r.rawId),
        RAW_ID, "rawId returned is the same");
    assert_true(r.response instanceof AuthenticatorAttestationResponse);
    assert_true(r.response.clientDataJSON instanceof ArrayBuffer);
    assert_array_equals(new Uint8Array(r.response.clientDataJSON),
        CLIENT_DATA_JSON, "clientDataJSON returned is the same");
    assert_true(r.response.attestationObject instanceof ArrayBuffer);
    assert_array_equals(new Uint8Array(r.response.attestationObject),
        ATTESTATION_OBJECT, "attestationObject returned is the same");
    assert_not_exists(r.response, 'authenticatorData');
    assert_not_exists(r.response, 'signature');
}

// Verifies if |r| is the valid response to credentials.get(publicKey).
function assertValidGetCredentialResponse(r) {
    assert_equals(r.id, ID, 'id');
    assert_true(r.rawId instanceof ArrayBuffer);
    assert_array_equals(new Uint8Array(r.rawId),
        RAW_ID, "rawId returned is the same");

    // The call returned an AssertionResponse, meaning it has
    //  authenticatorData and signature and does not have an attestationObject.
    assert_true(r.response instanceof AuthenticatorAssertionResponse);
    assert_true(r.response.clientDataJSON instanceof ArrayBuffer);
    assert_array_equals(new Uint8Array(r.response.clientDataJSON),
        CLIENT_DATA_JSON, "clientDataJSON returned is the same");
    assert_true(r.response.authenticatorData instanceof ArrayBuffer);
    assert_true(r.response.signature instanceof ArrayBuffer);
    assert_array_equals(new Uint8Array(r.response.authenticatorData),
        AUTHENTICATOR_DATA, "authenticator_data returned is the same");
    assert_array_equals(new Uint8Array(r.response.signature),
        SIGNATURE, "signature returned is the same");
    assert_not_exists(r.response, 'attestationObject');
}
