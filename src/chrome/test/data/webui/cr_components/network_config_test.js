// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('network-config', function() {
  var networkConfig;

  /** @type {NetworkingPrivate} */
  var api_;

  suiteSetup(function() {
    api_ = new chrome.FakeNetworkingPrivate();
    CrOncTest.overrideCrOncStrings();
  });

  function setNetworkConfig(properties) {
    PolymerTest.clearBody();
    networkConfig = document.createElement('network-config');
    networkConfig.networkingPrivate = api_;
    networkConfig.managedProperties =
        CrOncTest.convertToManagedProperties(properties);
  }

  function initNetworkConfig() {
    document.body.appendChild(networkConfig);
    networkConfig.init();
    Polymer.dom.flush();
  }

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => {
      networkConfig.async(resolve);
    });
  }

  suite('New WiFi Config', function() {
    setup(function() {
      api_.resetForTest();
      setNetworkConfig({GUID: '', Name: '', Type: 'WiFi'});
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
  });

  suite('Existing WiFi Config', function() {
    setup(function() {
      api_.resetForTest();
      var network = {
        GUID: 'someguid',
        Name: 'somename',
        Source: 'Device',
        Type: 'WiFi',
        WiFi: {SSID: 'somessid', Security: 'None'}
      };
      api_.addNetworksForTest([network]);
      setNetworkConfig({GUID: 'someguid', Name: '', Type: 'WiFi'});
      initNetworkConfig();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Default', function() {
      return flushAsync().then(() => {
        assertEquals('someguid', networkConfig.managedProperties.GUID);
        assertEquals(
            'somename',
            CrOnc.getActiveValue(networkConfig.managedProperties.Name));
        assertFalse(!!networkConfig.$$('#share'));
        assertTrue(!!networkConfig.$$('#ssid'));
        assertTrue(!!networkConfig.$$('#security'));
        assertTrue(networkConfig.$$('#security').disabled);
      });
    });
  });

  suite('Share', function() {
    setup(function() {
      api_.resetForTest();
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

    function setCertificatesForTest() {
      const kHash1 = 'TESTHASH1', kHash2 = 'TESTHASH2';
      var clientCert = {hash: kHash1, hardwareBacked: true, deviceWide: false};
      var caCert = {hash: kHash2, hardwareBacked: true, deviceWide: true};
      api_.setCertificatesForTest(
          {serverCaCertificates: [caCert], userCertificates: [clientCert]});
      this.selectedUserCertHash_ = kHash1;
      this.selectedServerCaHash_ = kHash2;
    }

    test('New Config: Login or guest', function() {
      // Insecure networks are always shared so test a secure config.
      setNetworkConfig(
          {GUID: '', Name: '', Type: 'WiFi', WiFi: {Security: 'WEP-PSK'}});
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
      setNetworkConfig(
          {GUID: '', Name: '', Type: 'WiFi', WiFi: {Security: 'WEP-PSK'}});
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
      setNetworkConfig(
          {GUID: '', Name: '', Type: 'WiFi', WiFi: {Security: 'None'}});
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
      setNetworkConfig(
          {GUID: '', Name: '', Type: 'WiFi', WiFi: {Security: 'WEP-PSK'}});
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertFalse(share.disabled);
        assertFalse(share.checked);
      });
    });

    // Existing networks hide the shared control in the config UI.
    test('Existing Hides Shared', function() {
      var network = {
        GUID: 'someguid',
        Name: 'somename',
        Source: 'User',
        Type: 'WiFi',
        WiFi: {SSID: 'somessid', Security: 'WEP-PSK'}
      };
      api_.addNetworksForTest([network]);
      setNetworkConfig({GUID: 'someguid', Name: '', Type: 'WiFi'});
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        assertFalse(!!networkConfig.$$('#share'));
      });
    });

    test('Ethernet', function() {
      var ethernet = {
        GUID: 'ethernetguid',
        Name: 'Ethernet',
        Type: 'Ethernet',
        Ethernet: {Authentication: 'None'}
      };
      api_.addNetworksForTest([ethernet]);
      setNetworkConfig({GUID: 'ethernetguid', Name: '', Type: 'Ethernet'});
      initNetworkConfig();
      return flushAsync().then(() => {
        assertEquals('ethernetguid', networkConfig.guid);
        assertEquals('None', networkConfig.security_);
        let outer = networkConfig.$$('#outer');
        assertFalse(!!outer);
      });
    });

    test('Ethernet EAP', function() {
      var ethernet = {
        GUID: 'ethernetguid',
        Name: 'Ethernet',
        Type: 'Ethernet',
        Ethernet: {Authentication: 'None'}
      };
      var ethernetEap = {
        GUID: 'eapguid',
        Name: 'EthernetEap',
        Type: 'Ethernet',
        Ethernet: {
          Authentication: '8021X',
          EAP: {Outer: 'PEAP'},
        }
      };
      api_.addNetworksForTest([ethernet, ethernetEap]);
      setNetworkConfig({GUID: 'ethernetguid', Name: '', Type: 'Ethernet'});
      initNetworkConfig();
      return flushAsync().then(() => {
        assertEquals('eapguid', networkConfig.guid);
        assertEquals('WPA-EAP', networkConfig.security_);
        assertEquals(
            'PEAP',
            CrOnc.getActiveValue(
                /** @type {chrome.networkingPrivate.ManagedDOMString|undefined} */
                (networkConfig.get(
                    'Ethernet.EAP.Outer', networkConfig.managedProperties))));
        let outer = networkConfig.$$('#outer');
        assertTrue(!!outer);
        assertTrue(!outer.disabled);
        assertEquals('PEAP', outer.value);
      });
    });

    test('WiFi EAP TLS', function() {
      var network = {
        GUID: 'eaptlsguid',
        Name: '',
        Type: 'WiFi',
        WiFi: {Security: 'WPA-EAP', EAP: {Outer: 'EAP-TLS'}}
      };
      api_.addNetworksForTest([network]);
      setNetworkConfig({GUID: 'eaptlsguid', Name: '', Type: 'WiFi'});
      setCertificatesForTest();
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        let outer = networkConfig.$$('#outer');
        assertEquals('EAP-TLS', outer.value);

        // check that a valid client user certificate is selected
        let clientCert = networkConfig.$$('#userCert').$$('select').value;
        assertTrue(!!clientCert);
        let caCert = networkConfig.$$('#serverCa').$$('select').value;
        assertTrue(!!caCert);

        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        // share the EAP TLS network
        share.checked = true;
        // trigger the onShareChanged_ event
        var event = new Event('change');
        share.dispatchEvent(event);
        // check that share is enabled
        assertTrue(share.checked);

        // check that client certificate selection is empty
        clientCert = networkConfig.$$('#userCert').$$('select').value;
        assertFalse(!!clientCert);
        // check that ca device-wide cert is still selected
        caCert = networkConfig.$$('#serverCa').$$('select').value;
        assertTrue(!!caCert);
      });
    });

  });

});
