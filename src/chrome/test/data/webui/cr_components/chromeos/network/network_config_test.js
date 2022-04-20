// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_config.m.js';

// #import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {eventToPromise} from '../../../test_util.js';
// clang-format on

suite('network-config', function() {
  var networkConfig;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  const kCaHash = 'CAHASH';
  const kUserHash1 = 'USERHASH1';
  const kUserHash2 = 'USERHASH2';
  const kCaPem = 'test-pem';
  const kUserCertId = 'test-cert-id';
  const kTestVpnName = 'test-vpn';
  const kTestVpnHost = 'test-vpn-host';
  const kTestUsername = 'test-username';
  const kTestPassword = 'test-password';
  const kTestPsk = 'test-psk';

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  function setNetworkConfig(properties) {
    assertTrue(!!properties.guid);
    mojoApi_.setManagedPropertiesForTest(properties);
    PolymerTest.clearBody();
    networkConfig = document.createElement('network-config');
    networkConfig.guid = properties.guid;
    networkConfig.managedProperties = properties;
  }

  function setNetworkType(type, security) {
    PolymerTest.clearBody();
    networkConfig = document.createElement('network-config');
    networkConfig.type = OncMojo.getNetworkTypeString(type);
    if (security !== undefined) {
      networkConfig.securityType_ = security;
    }
  }

  function initNetworkConfig() {
    document.body.appendChild(networkConfig);
    networkConfig.init();
    Polymer.dom.flush();
  }

  function initNetworkConfigWithCerts(hasServerCa, hasUserCert) {
    const serverCas = [];
    const userCerts = [];
    if (hasServerCa) {
      serverCas.push({
        hash: kCaHash,
        pemOrId: kCaPem,
        availableForNetworkAuth: true,
        hardwareBacked: true,
        deviceWide: true
      });
    }
    if (hasUserCert) {
      userCerts.push({
        hash: kUserHash1,
        pemOrId: kUserCertId,
        availableForNetworkAuth: true,
        hardwareBacked: true,
        deviceWide: false
      });
    }
    mojoApi_.setCertificatesForTest(serverCas, userCerts);
    initNetworkConfig();
  }

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => {
      networkConfig.async(resolve);
    });
  }

  /**
   * Simulate an element of id |elementId| fires enter event.
   * @param {string} elementId
   */
  function simulateEnterPressedInElement(elementId) {
    let element = networkConfig.$$(`#${elementId}`);
    networkConfig.connectOnEnter = true;
    assertTrue(!!element);
    element.fire('enter', {path: [element]});
  }

  suite('New WiFi Config', function() {
    setup(function() {
      mojoApi_.resetForTest();
      setNetworkType(chromeos.networkConfig.mojom.NetworkType.kWiFi);
      initNetworkConfig();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Default', function() {
      assertTrue(!!networkConfig.$$('#share'));
      assertTrue(!!networkConfig.$$('#ssid'));
      assertTrue(!!networkConfig.$$('#security'));
      assertFalse(networkConfig.$$('#security').disabled);
    });

    test('Passphrase field shows', function() {
      assertFalse(!!networkConfig.$$('#wifi-passphrase'));
      networkConfig.$$('#security').value =
          chromeos.networkConfig.mojom.SecurityType.kWpaPsk;
      return flushAsync().then(() => {
        assertTrue(!!networkConfig.$$('#wifi-passphrase'));
      });
    });
  });

  suite('Existing WiFi Config', function() {
    setup(function() {
      mojoApi_.resetForTest();
      const wifi1 = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kWiFi, 'someguid', '');
      wifi1.name = OncMojo.createManagedString('somename');
      wifi1.source = chromeos.networkConfig.mojom.OncSource.kDevice;
      wifi1.typeProperties.wifi.security =
          chromeos.networkConfig.mojom.SecurityType.kWepPsk;
      wifi1.typeProperties.wifi.ssid.activeValue = '11111111111';
      wifi1.typeProperties.wifi.passphrase = {activeValue: 'test_passphrase'};
      setNetworkConfig(wifi1);
      initNetworkConfig();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Default', function() {
      return flushAsync().then(() => {
        assertEquals('someguid', networkConfig.managedProperties.guid);
        assertEquals(
            'somename', networkConfig.managedProperties.name.activeValue);
        assertFalse(!!networkConfig.$$('#share'));
        assertTrue(!!networkConfig.$$('#ssid'));
        assertTrue(!!networkConfig.$$('#security'));
        assertTrue(networkConfig.$$('#security').disabled);
      });
    });

    test('WiFi input fires enter event on keydown', function() {
      return flushAsync().then(() => {
        assertFalse(networkConfig.propertiesSent_);
        simulateEnterPressedInElement('ssid');
        assertTrue(networkConfig.propertiesSent_);
      });
    });

    test('Remove error text when input key is pressed', function() {
      return flushAsync().then(() => {
        networkConfig.error = 'bad-passphrase';
        let passwordInput = networkConfig.$$('#wifi-passphrase');
        assertTrue(!!passwordInput);
        assertTrue(!!networkConfig.error);

        passwordInput.fire('keypress');
        Polymer.dom.flush();
        assertFalse(!!networkConfig.error);
      });
    });
  });

  suite('IKEv2', function() {
    setup(function() {
      mojoApi_.resetForTest();
      setNetworkType(chromeos.networkConfig.mojom.NetworkType.kVPN);
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    // Sets all mandatory fields for an IKEv2 VPN service.
    function setMandatoryFields() {
      const configProperties = networkConfig.get('configProperties_');
      configProperties.name = kTestVpnName;
      configProperties.typeConfig.vpn.host = kTestVpnHost;
    }

    // Checks that if fields are shown or hidden properly when switching
    // authentication type.
    test('Switch Authentication Type', function() {
      initNetworkConfig();

      networkConfig.set('vpnType_', 'IKEv2');
      Polymer.dom.flush();
      assertEquals(3, networkConfig.get('ipsecAuthTypeItems_').length);
      assertTrue(!!networkConfig.$$('#ipsec-auth-type'));
      assertFalse(!!networkConfig.$$('#l2tp-username-input'));

      assertEquals('EAP', networkConfig.ipsecAuthType_);
      assertFalse(!!networkConfig.$$('#ipsec-psk-input'));
      assertTrue(!!networkConfig.$$('#vpnServerCa'));
      assertFalse(!!networkConfig.$$('#vpnUserCert'));
      assertTrue(!!networkConfig.$$('#ipsec-eap-username-input'));
      assertTrue(!!networkConfig.$$('#ipsec-eap-password-input'));
      assertTrue(!!networkConfig.$$('#ipsec-local-id-input'));
      assertTrue(!!networkConfig.$$('#ipsec-remote-id-input'));

      networkConfig.set('ipsecAuthType_', 'PSK');
      Polymer.dom.flush();
      assertTrue(!!networkConfig.$$('#ipsec-psk-input'));
      assertFalse(!!networkConfig.$$('#vpnServerCa'));
      assertFalse(!!networkConfig.$$('#vpnUserCert'));
      assertFalse(!!networkConfig.$$('#ipsec-eap-username-input'));
      assertFalse(!!networkConfig.$$('#ipsec-eap-password-input'));
      assertTrue(!!networkConfig.$$('#ipsec-local-id-input'));
      assertTrue(!!networkConfig.$$('#ipsec-remote-id-input'));

      networkConfig.set('ipsecAuthType_', 'Cert');
      Polymer.dom.flush();
      assertFalse(!!networkConfig.$$('#ipsec-psk-input'));
      assertTrue(!!networkConfig.$$('#vpnServerCa'));
      assertTrue(!!networkConfig.$$('#vpnUserCert'));
      assertFalse(!!networkConfig.$$('#ipsec-eap-username-input'));
      assertFalse(!!networkConfig.$$('#ipsec-eap-password-input'));
      assertTrue(!!networkConfig.$$('#ipsec-local-id-input'));
      assertTrue(!!networkConfig.$$('#ipsec-remote-id-input'));
    });

    test('No Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ false, /* hasUserCert= */ false);
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          assertEquals('no-certs', networkConfig.selectedServerCaHash_);
          assertEquals('no-certs', networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty server CA and user cert.
          setMandatoryFields();
          assertFalse(networkConfig.vpnIsConfigured_());
        });
      });
    });

    test('No Server CA Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ false, /* hasUserCert= */ true);
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          assertEquals('no-certs', networkConfig.selectedServerCaHash_);
          assertEquals(kUserHash1, networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty server CA.
          setMandatoryFields();
          assertFalse(networkConfig.vpnIsConfigured_());
        });
      });
    });

    test('No Client Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ false);
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          assertEquals('no-certs', networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty client cert.
          setMandatoryFields();
          assertFalse(networkConfig.vpnIsConfigured_());
        });
      });
    });

    // Checks if the values of vpnIsConfigured_() and getPropertiesToSet_() are
    // correct when the authentication type is PSK.
    test('PSK', function() {
      initNetworkConfig();
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'PSK');
      Polymer.dom.flush();

      setMandatoryFields();
      const configProperties = networkConfig.get('configProperties_');
      assertFalse(networkConfig.vpnIsConfigured_());
      configProperties.typeConfig.vpn.ipSec.psk = kTestPsk;
      assertTrue(networkConfig.vpnIsConfigured_());

      let props = networkConfig.getPropertiesToSet_();
      const mojom = chromeos.networkConfig.mojom;
      assertEquals(kTestVpnName, props.name);
      assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
      assertEquals(mojom.VpnType.kIKEv2, props.typeConfig.vpn.type.value);
      assertEquals('PSK', props.typeConfig.vpn.ipSec.authenticationType);
      assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
      assertFalse(props.typeConfig.vpn.ipSec.saveCredentials);
      assertEquals(kTestPsk, props.typeConfig.vpn.ipSec.psk);
      assertEquals('', props.typeConfig.vpn.ipSec.localIdentity);
      assertEquals('', props.typeConfig.vpn.ipSec.remoteIdentity);

      networkConfig.set('vpnSaveCredentials_', true);
      assertTrue(networkConfig.getPropertiesToSet_()
                     .typeConfig.vpn.ipSec.saveCredentials);

      configProperties.typeConfig.vpn.ipSec.localIdentity = 'local-id';
      configProperties.typeConfig.vpn.ipSec.remoteIdentity = 'remote-id';
      props = networkConfig.getPropertiesToSet_();
      assertEquals('local-id', props.typeConfig.vpn.ipSec.localIdentity);
      assertEquals('remote-id', props.typeConfig.vpn.ipSec.remoteIdentity);
    });

    // Checks if values are read correctly for an existing service of PSK
    // authentication.
    test('Existing PSK', function() {
      const mojom = chromeos.networkConfig.mojom;
      const ikev2 = OncMojo.getDefaultManagedProperties(
          mojom.NetworkType.kVPN, 'someguid', kTestVpnName);
      ikev2.typeProperties.vpn.type = mojom.VpnType.kIKEv2;
      ikev2.typeProperties.vpn.host = {activeValue: kTestVpnHost};
      ikev2.typeProperties.vpn.ipSec = {
        authenticationType: {activeValue: 'PSK'},
        ikeVersion: {activeValue: 2},
        localIdentity: {activeValue: 'local-id'},
        remoteIdentity: {activeValue: 'remote-id'},
        saveCredentials: {activeValue: true},
      };
      setNetworkConfig(ikev2);
      initNetworkConfig();

      return flushAsync().then(() => {
        assertEquals('IKEv2', networkConfig.get('vpnType_'));
        assertEquals('PSK', networkConfig.get('ipsecAuthType_'));

        // Populate the properties again. The values should be the same to what
        // are set above.
        const props = networkConfig.getPropertiesToSet_();
        assertEquals('someguid', props.guid);
        assertEquals(kTestVpnName, props.name);
        assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
        assertEquals(mojom.VpnType.kIKEv2, props.typeConfig.vpn.type.value);
        assertEquals('PSK', props.typeConfig.vpn.ipSec.authenticationType);
        assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
        assertEquals('local-id', props.typeConfig.vpn.ipSec.localIdentity);
        assertEquals('remote-id', props.typeConfig.vpn.ipSec.remoteIdentity);
        assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
      });
    });

    // Checks if the values of vpnIsConfigured_() and getPropertiesToSet_() are
    // correct when the authentication type is user certificate.
    test('Cert', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ true);
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          // The first Server CA and User certificate should be selected.
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          assertEquals(kUserHash1, networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be true.
          setMandatoryFields();
          assertTrue(networkConfig.vpnIsConfigured_());

          const props = networkConfig.getPropertiesToSet_();
          const mojom = chromeos.networkConfig.mojom;
          assertEquals(kTestVpnName, props.name);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertEquals(mojom.VpnType.kIKEv2, props.typeConfig.vpn.type.value);
          assertEquals('Cert', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
          assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
          assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
          assertEquals('PKCS11Id', props.typeConfig.vpn.ipSec.clientCertType);
          assertEquals(
              kUserCertId, props.typeConfig.vpn.ipSec.clientCertPkcs11Id);
          assertFalse(props.typeConfig.vpn.ipSec.saveCredentials);
        });
      });
    });

    // Checks if values are read correctly for an existing service of
    // certificate authentication.
    test('Existing Cert', function() {
      const mojom = chromeos.networkConfig.mojom;
      const ikev2 = OncMojo.getDefaultManagedProperties(
          mojom.NetworkType.kVPN, 'someguid', kTestVpnName);
      ikev2.typeProperties.vpn.type = mojom.VpnType.kIKEv2;
      ikev2.typeProperties.vpn.host = {activeValue: kTestVpnHost};
      ikev2.typeProperties.vpn.ipSec = {
        authenticationType: {activeValue: 'Cert'},
        clientCertType: {activeValue: 'PKCS11Id'},
        clientCertPkcs11Id: {activeValue: kUserCertId},
        ikeVersion: {activeValue: 2},
        saveCredentials: {activeValue: true},
        serverCaPems: {activeValue: [kCaPem]},
      };
      setNetworkConfig(ikev2);
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ true);
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          assertEquals('IKEv2', networkConfig.get('vpnType_'));
          assertEquals('Cert', networkConfig.get('ipsecAuthType_'));
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          assertEquals(kUserHash1, networkConfig.selectedUserCertHash_);

          const props = networkConfig.getPropertiesToSet_();
          const mojom = chromeos.networkConfig.mojom;
          assertEquals('someguid', props.guid);
          assertEquals(kTestVpnName, props.name);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertEquals(mojom.VpnType.kIKEv2, props.typeConfig.vpn.type.value);
          assertEquals('Cert', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
          assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
          assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
          assertEquals('PKCS11Id', props.typeConfig.vpn.ipSec.clientCertType);
          assertEquals(
              kUserCertId, props.typeConfig.vpn.ipSec.clientCertPkcs11Id);
          assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
        });
      });
    });

    test('EAP', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ false);
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'EAP');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          // Server CA should be selected.
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);

          setMandatoryFields();
          assertFalse(networkConfig.vpnIsConfigured_());
          const eapProperties = networkConfig.get('eapProperties_');
          eapProperties.identity = kTestUsername;
          eapProperties.password = kTestPassword;
          assertTrue(networkConfig.vpnIsConfigured_());

          // Server CA is also mandatory when using EAP.
          networkConfig.set('selectedServerCaHash_', '');
          assertFalse(networkConfig.vpnIsConfigured_());
          networkConfig.set('selectedServerCaHash_', kCaHash);

          let props = networkConfig.getPropertiesToSet_();
          const mojom = chromeos.networkConfig.mojom;
          assertEquals(kTestVpnName, props.name);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertEquals(mojom.VpnType.kIKEv2, props.typeConfig.vpn.type.value);
          assertEquals('EAP', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
          assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
          assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
          assertEquals('MSCHAPv2', props.typeConfig.vpn.ipSec.eap.outer);
          assertEquals(kTestUsername, props.typeConfig.vpn.ipSec.eap.identity);
          assertEquals(kTestPassword, props.typeConfig.vpn.ipSec.eap.password);
          assertFalse(props.typeConfig.vpn.ipSec.saveCredentials);
          assertFalse(props.typeConfig.vpn.ipSec.eap.saveCredentials);

          networkConfig.set('vpnSaveCredentials_', true);
          props = networkConfig.getPropertiesToSet_();
          assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
          assertTrue(props.typeConfig.vpn.ipSec.eap.saveCredentials);
        });
      });
    });

    test('Existing EAP', function() {
      const mojom = chromeos.networkConfig.mojom;
      const ikev2 = OncMojo.getDefaultManagedProperties(
          mojom.NetworkType.kVPN, 'someguid', kTestVpnName);
      ikev2.typeProperties.vpn.type = mojom.VpnType.kIKEv2;
      ikev2.typeProperties.vpn.host = {activeValue: kTestVpnHost};
      ikev2.typeProperties.vpn.ipSec = {
        authenticationType: {activeValue: 'EAP'},
        eap: {
          domainSuffixMatch: {activeValue: []},
          identity: {activeValue: kTestUsername},
          outer: {activeValue: 'MSCHAPv2'},
          saveCredentials: {activeValue: true},
          subjectAltNameMatch: {activeValue: []},
          useSystemCas: {activeValue: false},
        },
        ikeVersion: {activeValue: 2},
        saveCredentials: {activeValue: true},
        serverCaPems: {activeValue: [kCaPem]},
      };
      setNetworkConfig(ikev2);
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ false);
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          assertEquals('IKEv2', networkConfig.get('vpnType_'));
          assertEquals('EAP', networkConfig.get('ipsecAuthType_'));
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);

          const props = networkConfig.getPropertiesToSet_();
          const mojom = chromeos.networkConfig.mojom;
          assertEquals('someguid', props.guid);
          assertEquals(kTestVpnName, props.name);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertEquals(mojom.VpnType.kIKEv2, props.typeConfig.vpn.type.value);
          assertEquals('EAP', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
          assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
          assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
          assertEquals('MSCHAPv2', props.typeConfig.vpn.ipSec.eap.outer);
          assertEquals(kTestUsername, props.typeConfig.vpn.ipSec.eap.identity);
          assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
          assertTrue(props.typeConfig.vpn.ipSec.eap.saveCredentials);
        });
      });
    });
  });

  suite('L2TP/IPsec', function() {
    setup(function() {
      mojoApi_.resetForTest();
      setNetworkType(chromeos.networkConfig.mojom.NetworkType.kVPN);
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    // Sets all mandatory fields for an L2TP/IPsec service except for server CA
    // and user certificate.
    function setMandatoryFields() {
      const configProperties = networkConfig.get('configProperties_');
      configProperties.name = kTestVpnName;
      configProperties.typeConfig.vpn.host = kTestVpnHost;
      configProperties.typeConfig.vpn.l2tp.username = kTestUsername;
      configProperties.typeConfig.vpn.l2tp.password = kTestPassword;
    }

    test('Switch Authentication Type', function() {
      initNetworkConfig();

      // Switch to L2TP/IPsec, the authentication type is default to PSK. The
      // PSK input should appear and the dropdowns for server CA and user
      // certificate should be hidden.
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      Polymer.dom.flush();
      assertEquals(2, networkConfig.get('ipsecAuthTypeItems_').length);
      assertEquals('PSK', networkConfig.ipsecAuthType_);
      assertFalse(!!networkConfig.$$('#ipsec-local-id-input'));
      assertFalse(!!networkConfig.$$('#ipsec-remote-id-input'));
      assertTrue(!!networkConfig.$$('#ipsec-auth-type'));
      assertTrue(!!networkConfig.$$('#l2tp-username-input'));
      assertTrue(!!networkConfig.$$('#ipsec-psk-input'));
      assertFalse(!!networkConfig.$$('#vpnServerCa'));
      assertFalse(!!networkConfig.$$('#vpnUserCert'));

      // Switch the authentication type to Cert. The PSK input should be hidden
      // and the dropdowns for server CA and user certificate should appear.
      networkConfig.set('ipsecAuthType_', 'Cert');
      Polymer.dom.flush();
      assertFalse(!!networkConfig.$$('#ipsec-psk-input'));
      assertTrue(!!networkConfig.$$('#ipsec-auth-type'));
      assertTrue(!!networkConfig.$$('#l2tp-username-input'));
      assertTrue(!!networkConfig.$$('#vpnServerCa'));
      assertTrue(!!networkConfig.$$('#vpnUserCert'));

      // Switch VPN type to IKEv2 and auth type to EAP, and then back to
      // L2TP/IPsec. The auth type should be reset to PSK since EAP is not a
      // valid value.
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'EAP');
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      assertEquals('PSK', networkConfig.ipsecAuthType_);
    });

    test('No Certs', function() {
      initNetworkConfig();
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          // Check that with no certificates, 'do-not-check' and 'no-certs' are
          // selected.
          assertEquals('no-certs', networkConfig.selectedServerCaHash_);
          assertEquals('no-certs', networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty server CA and user cert.
          setMandatoryFields();
          assertFalse(networkConfig.vpnIsConfigured_());
        });
      });
    });

    test('No Server CA Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ false, /* hasUserCert= */ true);
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          assertEquals('no-certs', networkConfig.selectedServerCaHash_);
          assertEquals(kUserHash1, networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty server CA.
          setMandatoryFields();
          assertFalse(networkConfig.vpnIsConfigured_());
        });
      });
    });

    test('No Client Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ false);
      initNetworkConfig();
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          assertEquals('no-certs', networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty client cert.
          setMandatoryFields();
          assertFalse(networkConfig.vpnIsConfigured_());
        });
      });
    });

    // Checks if the values of vpnIsConfigured_() and getPropertiesToSet_() are
    // correct when the authentication type is PSK.
    test('PSK', function() {
      initNetworkConfig();
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      Polymer.dom.flush();

      setMandatoryFields();
      const configProperties = networkConfig.get('configProperties_');
      assertFalse(networkConfig.vpnIsConfigured_());
      configProperties.typeConfig.vpn.ipSec.psk = kTestPsk;
      assertTrue(networkConfig.vpnIsConfigured_());

      let props = networkConfig.getPropertiesToSet_();
      const mojom = chromeos.networkConfig.mojom;
      assertEquals(kTestVpnName, props.name);
      assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
      assertEquals(mojom.VpnType.kL2TPIPsec, props.typeConfig.vpn.type.value);
      assertEquals('PSK', props.typeConfig.vpn.ipSec.authenticationType);
      assertEquals(1, props.typeConfig.vpn.ipSec.ikeVersion);
      assertFalse(props.typeConfig.vpn.ipSec.saveCredentials);
      assertEquals(kTestPsk, props.typeConfig.vpn.ipSec.psk);
      assertEquals(kTestUsername, props.typeConfig.vpn.l2tp.username);
      assertEquals(kTestPassword, props.typeConfig.vpn.l2tp.password);

      networkConfig.set('vpnSaveCredentials_', true);
      props = networkConfig.getPropertiesToSet_();
      assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
      assertTrue(props.typeConfig.vpn.l2tp.saveCredentials);
    });

    // Checks if values are read correctly for an existing service of PSK
    // authentication.
    test('Existing PSK', function() {
      const mojom = chromeos.networkConfig.mojom;
      const l2tp = OncMojo.getDefaultManagedProperties(
          mojom.NetworkType.kVPN, 'someguid', kTestVpnName);
      l2tp.typeProperties.vpn.type = mojom.VpnType.kL2TPIPsec;
      l2tp.typeProperties.vpn.host = {activeValue: kTestVpnHost};
      l2tp.typeProperties.vpn.ipSec = {
        authenticationType: {activeValue: 'PSK'},
        ikeVersion: {activeValue: 1},
        saveCredentials: {activeValue: true},
      };
      l2tp.typeProperties.vpn.l2tp = {
        username: {activeValue: kTestUsername},
        saveCredentials: {activeValue: true},
      };
      setNetworkConfig(l2tp);
      initNetworkConfig();

      return flushAsync().then(() => {
        assertEquals('L2TP_IPsec', networkConfig.get('vpnType_'));
        assertEquals('PSK', networkConfig.get('ipsecAuthType_'));

        // Populate the properties again. The values should be the same to what
        // are set above.
        const props = networkConfig.getPropertiesToSet_();
        assertEquals('someguid', props.guid);
        assertEquals(kTestVpnName, props.name);
        assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
        assertEquals(mojom.VpnType.kL2TPIPsec, props.typeConfig.vpn.type.value);
        assertEquals('PSK', props.typeConfig.vpn.ipSec.authenticationType);
        assertEquals(1, props.typeConfig.vpn.ipSec.ikeVersion);
        assertEquals(undefined, props.typeConfig.vpn.ipSec.eap);
        assertEquals(undefined, props.typeConfig.vpn.ipSec.localIdentity);
        assertEquals(undefined, props.typeConfig.vpn.ipSec.remoteIdentity);
        assertEquals(kTestUsername, props.typeConfig.vpn.l2tp.username);
        assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
        assertTrue(props.typeConfig.vpn.l2tp.saveCredentials);
      });
    });

    // Checks if the values of vpnIsConfigured_() and getPropertiesToSet_() are
    // correct when the authentication type is user certificate.
    test('Cert', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ true);
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          // The first Server CA and User certificate should be selected.
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          assertEquals(kUserHash1, networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be true.
          setMandatoryFields();
          assertTrue(networkConfig.vpnIsConfigured_());

          const props = networkConfig.getPropertiesToSet_();
          const mojom = chromeos.networkConfig.mojom;
          assertEquals(kTestVpnName, props.name);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertEquals(
              mojom.VpnType.kL2TPIPsec, props.typeConfig.vpn.type.value);
          assertEquals('Cert', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(1, props.typeConfig.vpn.ipSec.ikeVersion);
          assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
          assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
          assertEquals('PKCS11Id', props.typeConfig.vpn.ipSec.clientCertType);
          assertEquals(
              kUserCertId, props.typeConfig.vpn.ipSec.clientCertPkcs11Id);
          assertEquals(kTestUsername, props.typeConfig.vpn.l2tp.username);
          assertEquals(kTestPassword, props.typeConfig.vpn.l2tp.password);
          assertFalse(props.typeConfig.vpn.ipSec.saveCredentials);
          assertFalse(props.typeConfig.vpn.l2tp.saveCredentials);
        });
      });
    });

    // Checks if values are read correctly for an existing service of
    // certificate authentication.
    test('Existing Cert', function() {
      const mojom = chromeos.networkConfig.mojom;
      const l2tp = OncMojo.getDefaultManagedProperties(
          mojom.NetworkType.kVPN, 'someguid', kTestVpnName);
      l2tp.typeProperties.vpn.type = mojom.VpnType.kL2TPIPsec;
      l2tp.typeProperties.vpn.host = {activeValue: kTestVpnHost};
      l2tp.typeProperties.vpn.ipSec = {
        authenticationType: {activeValue: 'Cert'},
        clientCertType: {activeValue: 'PKCS11Id'},
        clientCertPkcs11Id: {activeValue: kUserCertId},
        ikeVersion: {activeValue: 1},
        saveCredentials: {activeValue: true},
        serverCaPems: {activeValue: [kCaPem]},
      };
      l2tp.typeProperties.vpn.l2tp = {
        username: {activeValue: kTestUsername},
        saveCredentials: {activeValue: true},
      };
      setNetworkConfig(l2tp);
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ true);
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        assertEquals('L2TP_IPsec', networkConfig.get('vpnType_'));
        assertEquals('Cert', networkConfig.get('ipsecAuthType_'));
        assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
        assertEquals(kUserHash1, networkConfig.selectedUserCertHash_);

        // Populate the properties again. The values should be the same to what
        // are set above.
        const props = networkConfig.getPropertiesToSet_();
        assertEquals('someguid', props.guid);
        assertEquals(kTestVpnName, props.name);
        assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
        assertEquals(mojom.VpnType.kL2TPIPsec, props.typeConfig.vpn.type.value);
        assertEquals('Cert', props.typeConfig.vpn.ipSec.authenticationType);
        assertEquals(1, props.typeConfig.vpn.ipSec.ikeVersion);
        assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
        assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
        assertEquals('PKCS11Id', props.typeConfig.vpn.ipSec.clientCertType);
        assertEquals(
            kUserCertId, props.typeConfig.vpn.ipSec.clientCertPkcs11Id);
        assertEquals(undefined, props.typeConfig.vpn.ipSec.eap);
        assertEquals(undefined, props.typeConfig.vpn.ipSec.localIdentity);
        assertEquals(undefined, props.typeConfig.vpn.ipSec.remoteIdentity);
        assertEquals(kTestUsername, props.typeConfig.vpn.l2tp.username);
        assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
        assertTrue(props.typeConfig.vpn.l2tp.saveCredentials);
      });
    });
  });

  suite('OpenVPN', function() {
    setup(function() {
      mojoApi_.resetForTest();
      setNetworkType(chromeos.networkConfig.mojom.NetworkType.kVPN);
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Switch VPN Type', function() {
      initNetworkConfig();

      // Default VPN type is OpenVPN. Verify the displayed items.
      assertEquals('OpenVPN', networkConfig.get('vpnType_'));
      assertFalse(!!networkConfig.$$('#ipsec-auth-type'));
      assertFalse(!!networkConfig.$$('#l2tp-username-input'));
      assertTrue(!!networkConfig.$$('#openvpn-username-input'));
      assertTrue(!!networkConfig.$$('#vpnServerCa'));
      assertTrue(!!networkConfig.$$('#vpnUserCert'));

      // Switch the VPN type to another and back again. Items should not change.
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      Polymer.dom.flush();
      networkConfig.set('vpnType_', 'OpenVPN');
      Polymer.dom.flush();
      assertFalse(!!networkConfig.$$('#ipsec-auth-type'));
      assertFalse(!!networkConfig.$$('#l2tp-username-input'));
      assertTrue(!!networkConfig.$$('#openvpn-username-input'));
      assertTrue(!!networkConfig.$$('#vpnServerCa'));
      assertTrue(!!networkConfig.$$('#vpnUserCert'));
    });

    test('No Certs', function() {
      initNetworkConfig();
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          // Check that with no certificates, 'do-not-check' and 'no-user-certs'
          // are selected.
          assertEquals('do-not-check', networkConfig.selectedServerCaHash_);
          assertEquals('no-user-cert', networkConfig.selectedUserCertHash_);
        });
      });
    });

    test('Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ true);
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          // The first Server CA should be selected.
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          // OpenVPN allows but does not require a user certificate.
          assertEquals('no-user-cert', networkConfig.selectedUserCertHash_);
        });
      });
    });
  });

  suite('WireGuard', function() {
    setup(function() {
      mojoApi_.resetForTest();
      setNetworkType(chromeos.networkConfig.mojom.NetworkType.kVPN);
      initNetworkConfig();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Switch VPN Type', function() {
      const configProperties = networkConfig.get('configProperties_');
      networkConfig.set('vpnType_', 'OpenVPN');
      Polymer.dom.flush();
      assertFalse(!!configProperties.typeConfig.vpn.wireguard);
      assertFalse(!!networkConfig.$$('#wireguard-ip-input'));
      networkConfig.set('vpnType_', 'WireGuard');
      Polymer.dom.flush();
      assertFalse(!!configProperties.typeConfig.vpn.openvpn);
      assertTrue(!!configProperties.typeConfig.vpn.wireguard);
      assertTrue(!!networkConfig.$$('#wireguard-ip-input'));
    });

    test('Switch key config type', function() {
      networkConfig.set('vpnType_', 'WireGuard');
      Polymer.dom.flush();
      assertFalse(!!networkConfig.$$('#wireguardPrivateKeyInput'));
      networkConfig.set('wireguardKeyType_', 'UserInput');
      return flushAsync().then(() => {
        assertTrue(!!networkConfig.$$('#wireguardPrivateKeyInput'));
      });
    });

    test('Enable Connect', function() {
      networkConfig.set('vpnType_', 'WireGuard');
      Polymer.dom.flush();
      assertFalse(networkConfig.enableConnect);
      networkConfig.set('ipAddressInput_', '10.10.0.1');
      const configProperties = networkConfig.get('configProperties_');
      configProperties.name = 'test-wireguard';
      const peer = configProperties.typeConfig.vpn.wireguard.peers[0];
      peer.publicKey = 'KFhwdv4+jKpSXMW6xEUVtOe4Mo8l/xOvGmshmjiHx1Y=';
      assertFalse(networkConfig.vpnIsConfigured_());
      peer.endpoint = '192.168.66.66:32000';
      peer.allowedIps = '0.0.0.0/0';
      assertTrue(networkConfig.vpnIsConfigured_());
      peer.presharedKey = 'invalid_key';
      assertFalse(networkConfig.vpnIsConfigured_());
      peer.presharedKey = '';
      assertTrue(networkConfig.vpnIsConfigured_());
    });
  });

  suite('Existing WireGuard', function() {
    setup(function() {
      mojoApi_.resetForTest();
      const wg1 = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kVPN, 'someguid', '');
      wg1.typeProperties.vpn.type =
          chromeos.networkConfig.mojom.VpnType.kWireGuard;
      wg1.typeProperties.vpn.wireguard = {
        peers: {
          activeValue: [{
            publicKey: 'KFhwdv4+jKpSXMW6xEUVtOe4Mo8l/xOvGmshmjiHx1Y=',
            endpoint: '192.168.66.66:32000',
            allowedIps: '0.0.0.0/0',
          }]
        }
      };
      wg1.staticIpConfig = {ipAddress: {activeValue: '10.10.0.1'}};
      setNetworkConfig(wg1);
      initNetworkConfig();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Value Reflected', function() {
      return flushAsync().then(() => {
        const configProperties = networkConfig.get('configProperties_');
        const peer = configProperties.typeConfig.vpn.wireguard.peers[0];
        assertEquals('UseCurrent', networkConfig.wireguardKeyType_);
        assertEquals('10.10.0.1', networkConfig.get('ipAddressInput_'));
        assertEquals(
            'KFhwdv4+jKpSXMW6xEUVtOe4Mo8l/xOvGmshmjiHx1Y=', peer.publicKey);
        assertEquals('192.168.66.66:32000', peer.endpoint);
        assertEquals('0.0.0.0/0', peer.allowedIps);
      });
    });

    test('Preshared key display and config value', function() {
      return flushAsync().then(() => {
        const configProperties = networkConfig.get('configProperties_');
        assertEquals(
            '(credential)',
            configProperties.typeConfig.vpn.wireguard.peers[0].presharedKey);
        const configToSet = networkConfig.getPropertiesToSet_();
        assertEquals(
            undefined,
            configToSet.typeConfig.vpn.wireguard.peers[0].presharedKey);
      });
    });
  });

  suite('Share', function() {
    setup(function() {
      mojoApi_.resetForTest();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    function setLoginOrGuest() {
      // Networks must be shared.
      networkConfig.shareAllowEnable = false;
      networkConfig.shareDefault = true;
    }

    function setKiosk() {
      // New networks can not be shared.
      networkConfig.shareAllowEnable = false;
      networkConfig.shareDefault = false;
    }

    function setAuthenticated() {
      // Logged in users can share new networks.
      networkConfig.shareAllowEnable = true;
      // Authenticated networks default to not shared.
      networkConfig.shareDefault = false;
    }

    test('New Config: Login or guest', function() {
      // Insecure networks are always shared so test a secure config.
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWepPsk);
      setLoginOrGuest();
      initNetworkConfig();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertTrue(share.checked);
      });
    });

    test('New Config: Kiosk', function() {
      // Insecure networks are always shared so test a secure config.
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWepPsk);
      setKiosk();
      initNetworkConfig();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertFalse(share.checked);
      });
    });

    test('New Config: Authenticated, Not secure', function() {
      setNetworkType(chromeos.networkConfig.mojom.NetworkType.kWiFi);
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertTrue(share.checked);
      });
    });

    test('New Config: Authenticated, Secure', function() {
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWepPsk);
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertFalse(share.disabled);
        assertFalse(share.checked);
      });
    });

    test('New Config: Authenticated, Not secure to secure', async function() {
      // set default to insecure network
      setNetworkType(chromeos.networkConfig.mojom.NetworkType.kWiFi);
      setAuthenticated();
      initNetworkConfig();
      await flushAsync();
      let share = networkConfig.$$('#share');
      assertTrue(!!share);
      assertTrue(share.disabled);
      assertTrue(share.checked);

      // change to secure network
      networkConfig.securityType_ =
          chromeos.networkConfig.mojom.SecurityType.kWepPsk;
      await flushAsync();
      assertTrue(!!share);
      assertFalse(share.disabled);
      assertFalse(share.checked);
    });

    // Existing networks hide the shared control in the config UI.
    test('Existing Hides Shared', function() {
      const wifi1 = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kWiFi, 'someguid', '');
      wifi1.source = chromeos.networkConfig.mojom.OncSource.kUser;
      wifi1.typeProperties.wifi.security =
          chromeos.networkConfig.mojom.SecurityType.kWepPsk;
      setNetworkConfig(wifi1);
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        assertFalse(!!networkConfig.$$('#share'));
      });
    });

    test('Ethernet', function() {
      const eth = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kEthernet, 'ethernetguid',
          '');
      eth.typeProperties.ethernet.authentication =
          OncMojo.createManagedString('None');
      setNetworkConfig(eth);
      initNetworkConfig();
      return flushAsync().then(() => {
        assertEquals('ethernetguid', networkConfig.guid);
        assertEquals(
            chromeos.networkConfig.mojom.SecurityType.kNone,
            networkConfig.securityType_);
        let outer = networkConfig.$$('#outer');
        assertFalse(!!outer);
      });
    });

    test('Ethernet EAP', function() {
      const eth = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kEthernet, 'eapguid', '');
      eth.typeProperties.ethernet.authentication =
          OncMojo.createManagedString('8021x');
      eth.typeProperties.ethernet.eap = {
        outer: OncMojo.createManagedString('PEAP')
      };
      setNetworkConfig(eth);
      initNetworkConfig();
      return flushAsync().then(() => {
        assertEquals('eapguid', networkConfig.guid);
        assertEquals(
            chromeos.networkConfig.mojom.SecurityType.kWpaEap,
            networkConfig.securityType_);
        assertEquals(
            'PEAP',
            networkConfig.managedProperties.typeProperties.ethernet.eap.outer
                .activeValue);
        assertEquals(
            'PEAP',
            networkConfig.configProperties_.typeConfig.ethernet.eap.outer);
        assertEquals('PEAP', networkConfig.eapProperties_.outer);
        let outer = networkConfig.$$('#outer');
        assertTrue(!!outer);
        assertTrue(!outer.disabled);
        assertEquals('PEAP', outer.value);
      });
    });

    test('Ethernet input fires enter event on keydown', function() {
      const eth = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kEthernet, 'eapguid', '');
      eth.typeProperties.ethernet.authentication =
          OncMojo.createManagedString('8021x');
      eth.typeProperties.ethernet.eap = {
        outer: OncMojo.createManagedString('PEAP')
      };
      setNetworkConfig(eth);
      initNetworkConfig();
      return flushAsync().then(() => {
        assertFalse(networkConfig.propertiesSent_);
        simulateEnterPressedInElement('oncEAPIdentity');
        assertTrue(networkConfig.propertiesSent_);
      });
    });
  });

  suite('Certificates', function() {
    setup(function() {
      mojoApi_.resetForTest();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    function setAuthenticated() {
      // Logged in users can share new networks.
      networkConfig.shareAllowEnable = true;
      // Authenticated networks default to not shared.
      networkConfig.shareDefault = false;
    }

    test('WiFi EAP-TLS No Certs', function() {
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWpaEap);
      setAuthenticated();
      initNetworkConfig();
      networkConfig.shareNetwork_ = false;
      networkConfig.set('eapProperties_.outer', 'EAP-TLS');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          let outer = networkConfig.$$('#outer');
          assertEquals('EAP-TLS', outer.value);
          // Check that with no certificates, 'default' and 'no-certs' are
          // selected.
          assertEquals('default', networkConfig.selectedServerCaHash_);
          assertEquals('no-certs', networkConfig.selectedUserCertHash_);
        });
      });
    });

    test('WiFi EAP-TLS Certs', function() {
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWpaEap);
      setAuthenticated();
      mojoApi_.setCertificatesForTest(
          [{
            hash: kCaHash,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: true
          }],
          [{
            hash: kUserHash1,
            pemOrId: kUserCertId,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: false
          }]);
      initNetworkConfig();
      networkConfig.shareNetwork_ = false;
      networkConfig.set('eapProperties_.outer', 'EAP-TLS');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          // The first Server CA  and User certificate should be selected.
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          assertEquals(kUserHash1, networkConfig.selectedUserCertHash_);
        });
      });
    });

    test('WiFi EAP-TLS Certs Shared', function() {
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWpaEap);
      setAuthenticated();
      mojoApi_.setCertificatesForTest(
          [{
            hash: kCaHash,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: true
          }],
          [
            {
              hash: kUserHash1,
              pemOrId: kUserCertId,
              availableForNetworkAuth: true,
              hardwareBacked: true,
              deviceWide: false
            },
            {
              hash: kUserHash2,
              pemOrId: kUserCertId,
              availableForNetworkAuth: true,
              hardwareBacked: true,
              deviceWide: true
            }
          ]);
      initNetworkConfig();
      networkConfig.shareNetwork_ = true;
      networkConfig.set('eapProperties_.outer', 'EAP-TLS');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          // The first Server CA should be selected.
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          // Second User Hash should be selected since it is a device cert.
          assertEquals(kUserHash2, networkConfig.selectedUserCertHash_);
        });
      });
    });
  });
});
