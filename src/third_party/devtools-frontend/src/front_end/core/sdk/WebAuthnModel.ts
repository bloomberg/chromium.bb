// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type * as ProtocolProxyApi from '../../generated/protocol-proxy-api.js';
import type * as Protocol from '../../generated/protocol.js';

import type {Target} from './Target.js';
import {Capability} from './Target.js';
import {SDKModel} from './SDKModel.js';

export class WebAuthnModel extends SDKModel {
  private readonly agent: ProtocolProxyApi.WebAuthnApi;
  constructor(target: Target) {
    super(target);
    this.agent = target.webAuthnAgent();
  }

  setVirtualAuthEnvEnabled(enable: boolean): Promise<Object> {
    if (enable) {
      return this.agent.invoke_enable();
    }
    return this.agent.invoke_disable();
  }

  async addAuthenticator(options: Protocol.WebAuthn.VirtualAuthenticatorOptions):
      Promise<Protocol.WebAuthn.AuthenticatorId> {
    const response = await this.agent.invoke_addVirtualAuthenticator({options});
    return response.authenticatorId;
  }

  async removeAuthenticator(authenticatorId: Protocol.WebAuthn.AuthenticatorId): Promise<void> {
    await this.agent.invoke_removeVirtualAuthenticator({authenticatorId});
  }

  async setAutomaticPresenceSimulation(authenticatorId: Protocol.WebAuthn.AuthenticatorId, enabled: boolean):
      Promise<void> {
    await this.agent.invoke_setAutomaticPresenceSimulation({authenticatorId, enabled});
  }

  async getCredentials(authenticatorId: Protocol.WebAuthn.AuthenticatorId): Promise<Protocol.WebAuthn.Credential[]> {
    const response = await this.agent.invoke_getCredentials({authenticatorId});
    return response.credentials;
  }

  async removeCredential(authenticatorId: Protocol.WebAuthn.AuthenticatorId, credentialId: string): Promise<void> {
    await this.agent.invoke_removeCredential({authenticatorId, credentialId});
  }
}

SDKModel.register(WebAuthnModel, {capabilities: Capability.WebAuthn, autostart: false});
