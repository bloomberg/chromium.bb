// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier.signin;

import android.accounts.Account;
import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.test.util.AccountHolder;
import org.chromium.sync.test.util.MockAccountManager;

/**
 * Test class for {@link AccountManagerHelper}.
 */
public class AccountManagerHelperTest extends InstrumentationTestCase {

    private MockAccountManager mAccountManager;
    private AccountManagerHelper mHelper;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        Context context = getInstrumentation().getTargetContext();
        mAccountManager = new MockAccountManager(context, context);
        AccountManagerHelper.overrideAccountManagerHelperForTests(context, mAccountManager);
        mHelper = AccountManagerHelper.get(context);
    }

    private Account addTestAccount(String accountName, String password) {
        Account account = AccountManagerHelper.createAccountFromName(accountName);
        AccountHolder.Builder accountHolder =
                AccountHolder.create().account(account).password(password).alwaysAccept(true);
        mAccountManager.addAccountHolderExplicitly(accountHolder.build());
        return account;
    }

    @SmallTest
    public void testCanonicalAccount() throws InterruptedException {
        addTestAccount("test@gmail.com", "password");

        assertTrue(mHelper.hasAccountForName("test@gmail.com"));
        assertTrue(mHelper.hasAccountForName("Test@gmail.com"));
        assertTrue(mHelper.hasAccountForName("te.st@gmail.com"));
    }

    @SmallTest
    public void testNonCanonicalAccount() throws InterruptedException {
        addTestAccount("test.me@gmail.com", "password");

        assertTrue(mHelper.hasAccountForName("test.me@gmail.com"));
        assertTrue(mHelper.hasAccountForName("testme@gmail.com"));
        assertTrue(mHelper.hasAccountForName("Testme@gmail.com"));
        assertTrue(mHelper.hasAccountForName("te.st.me@gmail.com"));
    }
}
