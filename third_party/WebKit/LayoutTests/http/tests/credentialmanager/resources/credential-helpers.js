'use strict';

// Converts an ECMAScript String object to an instance of
// mojo_base.mojom.String16.
function stringToMojoString16(string) {
  let array = new Array(string.length);
  for (var i = 0; i < string.length; ++i) {
    array[i] = string.charCodeAt(i);
  }
  return { data: array }
}

// Mocks the CredentialManager interface defined in credential_manager.mojom.
class MockCredentialManager {
  constructor() {
    this.reset();

    this.binding_ = new mojo.Binding(passwordManager.mojom.CredentialManager, this);
    this.interceptor_ = new MojoInterfaceInterceptor(
        passwordManager.mojom.CredentialManager.name);
    this.interceptor_.oninterfacerequest = e => {
      this.binding_.bind(e.handle);
    };
    this.interceptor_.start();
  }

  constructCredentialInfo_(type, id, password, name, icon) {
  return new passwordManager.mojom.CredentialInfo({
      type: type,
      id: stringToMojoString16(id),
      name: stringToMojoString16(name),
      icon: new url.mojom.Url({url: icon}),
      password: stringToMojoString16(password),
      federation: new url.mojom.Origin(
          {scheme: '', host: '', port: 0, unique: true})
    });
  }

  // Mock functions:

  async get(mediation, includePasswords, federations) {
    console.log(passwordManager.mojom.CredentialType.PASSWORD);
    if (this.error_ == passwordManager.mojom.CredentialManagerError.SUCCESS) {
      return {error: this.error_, credential: this.credentialInfo_};
    } else {
      return {error: this.error_, credential: null};
    }
  }

  async store(credential) {
    return {};
  }

  async preventSilentAccess() {
    return {};
  }

  // Resets state of mock CredentialManager.
  reset() {
    this.error_ = passwordManager.mojom.CredentialManagerError.SUCCESS;
    this.credentialInfo_ = this.constructCredentialInfo_(
        passwordManager.mojom.CredentialType.EMPTY, '', '', '', '');
  }

  setResponse(id, password, name, icon) {
    this.credentialInfo_ = this.constructCredentialInfo_(
        passwordManager.mojom.CredentialType.PASSWORD, id, password, name,
        icon);
  }

  setError(error) {
    this.error_ = error;
  }
}

// Class that mocks Authenticator interface defined in authenticator.mojom.
class MockAuthenticator {
  constructor() {
    this.reset();

    this.binding_ = new mojo.Binding(webauth.mojom.Authenticator, this);
    this.interceptor_ = new MojoInterfaceInterceptor(
        webauth.mojom.Authenticator.name);
    this.interceptor_.oninterfacerequest = e => {
      this.binding_.bind(e.handle);
    };
    this.interceptor_.start();
  }

  // Returns a MakeCredentialResponse to the client.
  async makeCredential(options) {
    var response = null;
    if (this.status_ == webauth.mojom.AuthenticatorStatus.SUCCESS) {
      let info = new webauth.mojom.CommonCredentialInfo(
          { id: this.id_,
            rawId: this.rawId_,
            clientDataJson: this.clientDataJson_,
          });
      response = new webauth.mojom.MakeCredentialAuthenticatorResponse(
          { info: info,
            attestationObject: this.attestationObject_
          });
    }
    let status = this.status_;
    this.reset();
    return {status, credential: response};
  }

  async getAssertion(options) {
    var response = null;
    if (this.status_ == webauth.mojom.AuthenticatorStatus.SUCCESS) {
      let info = new webauth.mojom.CommonCredentialInfo(
          { id: this.id_,
            rawId: this.rawId_,
            clientDataJson: this.clientDataJson_,
          });
      response = new webauth.mojom.GetAssertionAuthenticatorResponse(
          { info: info,
            authenticatorData: this.authenticatorData_,
            signature: this.signature_,
            userHandle: this.userHandle_,
          });
    }
    let status = this.status_;
    this.reset();
    return {status, credential: response};
  }

  // Resets state of mock Authenticator.
  reset() {
    this.status_ = webauth.mojom.AuthenticatorStatus.UNKNOWN_ERROR;
    this.id_ = null;
    this.rawId_ = new Uint8Array(0);
    this.clientDataJson_ = new Uint8Array(0);
    this.attestationObject_ = new Uint8Array(0);
    this.authenticatorData_ = new Uint8Array(0);
    this.signature_ = new Uint8Array(0);
    this.userHandle_ = new Uint8Array(0);
  }

  // Sets everything needed for a MakeCredential success response.
  setDefaultsForSuccessfulMakeCredential() {
    mockAuthenticator.setRawId(RAW_ID);
    mockAuthenticator.setId(ID);
    mockAuthenticator.setClientDataJson(CLIENT_DATA_JSON);
    mockAuthenticator.setAttestationObject(ATTESTATION_OBJECT);
    mockAuthenticator.setAuthenticatorStatus(
        webauth.mojom.AuthenticatorStatus.SUCCESS);
  }

  // Sets everything needed for a GetAssertion success response.
  setDefaultsForSuccessfulGetAssertion() {
    mockAuthenticator.setRawId(RAW_ID);
    mockAuthenticator.setId(ID);
    mockAuthenticator.setClientDataJson(CLIENT_DATA_JSON);
    mockAuthenticator.setAuthenticatorData(AUTHENTICATOR_DATA);
    mockAuthenticator.setSignature(SIGNATURE);
    mockAuthenticator.setAuthenticatorStatus(
        webauth.mojom.AuthenticatorStatus.SUCCESS);
  }

  setAuthenticatorStatus(status) {
    this.status_ = status;
  }

  setId(id) {
    this.id_ = id;
  }

  setRawId(rawId) {
    this.rawId_ = rawId;
  }

  setClientDataJson(clientDataJson) {
    this.clientDataJson_ = clientDataJson;
  }

  setAttestationObject(attestationObject) {
    this.attestationObject_ = attestationObject;
  }

  setAuthenticatorData(authenticatorData) {
    this.authenticatorData_ = authenticatorData;
  }

  setSignature(signature) {
    this.signature_ = signature;
  }

  setUserHandle(userHandle) {
    this.userHandle_ = userHandle;
  }
}

var mockAuthenticator = new MockAuthenticator();
var mockCredentialManager = new MockCredentialManager();

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
