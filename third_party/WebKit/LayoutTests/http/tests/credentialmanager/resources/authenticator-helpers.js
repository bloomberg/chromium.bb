'use strict';

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

  // Returns a PublicKeyCredentialInfo to the client.
  async makeCredential(options) {
    var info = null;
    if (this.status_ == webauth.mojom.AuthenticatorStatus.SUCCESS) {
      let response = new webauth.mojom.AuthenticatorResponse(
          { attestationObject: this.attestationObject_,
            authenticatorData: this.authenticatorData_,
            signature: this.signature_
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

  // Mock functions

  // Resets state of mock Authenticator.
  reset() {
    this.status_ = webauth.mojom.AuthenticatorStatus.UNKNOWN_ERROR;
    this.id_ = null;
    this.rawId_ = new Uint8Array(0);
    this.clientDataJson_ = new Uint8Array(0);
    this.attestationObject_ = new Uint8Array(0);
    this.authenticatorData_ = new Uint8Array(0);
    this.signature_ = new Uint8Array(0);
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
}

let mockAuthenticator = new MockAuthenticator();
