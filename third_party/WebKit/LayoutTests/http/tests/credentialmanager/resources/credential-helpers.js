'use strict';

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
      id: id,
      name: name,
      icon: new url.mojom.Url({url: icon}),
      password: password,
      federation: new url.mojom.Origin(
          {scheme: '', host: '', port: 0, suborigin: '', unique: true})
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

  // Mock functions:

  async makeCredential(options) {
    var info = null;
    if (this.status_ == webauth.mojom.AuthenticatorStatus.SUCCESS) {
      let response = new webauth.mojom.AuthenticatorResponse(
          { attestationObject: this.attestationObject_,
            authenticatorData: this.authenticatorData_,
            signature: this.signature_,
            userHandle: this.userHandle_
          });
      info = new webauth.mojom.PublicKeyCredentialInfo(
          { id: this.id_,
            rawId: this.rawId_,
            clientDataJson: this.clientDataJson_,
            response: response
          });
    }
    let status = this.status_;
    this.reset();
    return {status, credential: info};
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
    this.userHandle = new Uint8Array(0);
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
    this.userHandle = userHandle;
  }
}

var mockAuthenticator = new MockAuthenticator();
var mockCredentialManager = new MockCredentialManager();
