// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page_account_manager', function() {
  /** @implements {settings.AccountManagerBrowserProxy} */
  class TestAccountManagerBrowserProxy extends TestBrowserProxy {
    constructor() {
      super([
        'getAccounts',
        'addAccount',
        'removeAccount',
      ]);
    }

    /** @override */
    getAccounts() {
      this.methodCalled('getAccounts');

      return new Promise((resolve) => {
        resolve([
          {
            id: '123',
            accountType: 1,
            isDeviceAccount: true,
            fullName: 'Device Account',
            email: 'admin@domain.com',
            pic: 'data:image/png;base64,abc123',
          },
          {
            id: '456',
            accountType: 1,
            isDeviceAccount: false,
            fullName: 'Secondary Account',
            email: 'user@domain.com',
            pic: '',
          },
        ]);
      });
    }

    /** @override */
    addAccount() {
      this.methodCalled('addAccount');
    }

    /** @override */
    removeAccount(account) {
      this.methodCalled('removeAccount', account);
    }
  }

  suite('AccountManagerTests', function() {
    let browserProxy = null;
    let accountManager = null;
    let accountList = null;

    suiteSetup(function() {});

    setup(function() {
      browserProxy = new TestAccountManagerBrowserProxy();
      settings.AccountManagerBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();

      accountManager = document.createElement('settings-account-manager');
      document.body.appendChild(accountManager);
      accountList = accountManager.$$('#account-list');
      assertTrue(!!accountList);

      settings.navigateTo(settings.routes.ACCOUNT_MANAGER);
    });

    teardown(function() {
      accountManager.remove();
    });

    test('AccountListIsPopulatedAtStartup', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        // 2 accounts were added in |getAccounts()| mock above.
        assertEquals(2, accountList.items.length);
      });
    });

    test('AddAccount', function() {
      accountManager.$$('#add-account-button').click();
      assertEquals(1, browserProxy.getCallCount('addAccount'));
    });

    test('RemoveAccount', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        // Click on 'More Actions' for the second account
        accountManager.root.querySelectorAll('paper-icon-button-light')[1]
            .querySelector('button')
            .click();
        // Click on 'Remove account'
        accountManager.$$('cr-action-menu').querySelector('button').click();

        return browserProxy.whenCalled('removeAccount').then((account) => {
          assertEquals('456', account.id);
        });
      });
    });

    test('AccountListIsUpdatedWhenAccountManagerUpdates', function() {
      assertEquals(1, browserProxy.getCallCount('getAccounts'));
      cr.webUIListenerCallback('accounts-changed');
      assertEquals(2, browserProxy.getCallCount('getAccounts'));
    });
  });
});
