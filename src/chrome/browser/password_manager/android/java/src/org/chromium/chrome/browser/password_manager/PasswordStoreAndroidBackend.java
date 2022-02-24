// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.accounts.Account;

import com.google.common.base.Optional;

import org.chromium.base.Callback;

/**
 * Interface to send backend requests to a downstream implementation to fulfill password store
 * jobs. All methods are expected to respond asynchronously to callbacks.
 */
public interface PasswordStoreAndroidBackend {
    /**
     * Serves as a general exception for failed requests to the PasswordStoreAndroidBackend.
     */
    public class BackendException extends Exception {
        public @AndroidBackendErrorType int errorCode;

        public BackendException(String message, @AndroidBackendErrorType int error) {
            super(message);
            errorCode = error;
        }
    }

    /**
     * Subscribes to notifications from the downstream implementation.
     *
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * TODO(crbug.com/1278767): Remove default keyword after downstream implementation.
     */
    default void subscribe(Runnable successCallback, Callback<Exception> failureCallback){};

    /**
     * Unsubscribes from notifications from the downstream implementation.
     *
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * TODO(crbug.com/1278767): Remove default keyword after downstream implementation.
     */
    default void unsubscribe(Runnable successCallback, Callback<Exception> failureCallback){};

    /**
     * Triggers an async list call to retrieve all logins.
     *
     * @param target enum {@link StoreOperationTarget} to decide which storage to use.
     * @param loginsReply Callback that is called on success with serialized {@link
     *         org.chromium.components.sync.protocol.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * @deprecated use {@link #getAllLogins(Optional<Account>, Callback<byte[]>,
     *         Callback<Exception>)}.
     */
    default void getAllLogins(@PasswordStoreOperationTarget int target,
            Callback<byte[]> loginsReply, Callback<Exception> failureCallback){};

    /**
     * Triggers an async list call to retrieve all logins.
     *
     * @param syncingAccount Account used to sync passwords. If the syncingAccount is empty local
     *         account will be used.
     * @param loginsReply Callback that is called on success with serialized {@link
     *         org.chromium.components.sync.protocol.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * TODO(crbug.com/1278767): Remove default keyword after downstream implementation is ready.
     */
    default void getAllLogins(Optional<Account> syncingAccount, Callback<byte[]> loginsReply,
            Callback<Exception> failureCallback){};

    /**
     * Triggers an async list call to retrieve autofillable logins.
     *
     * @param loginsReply Callback that is called on success with serialized {@link
     *         org.chromium.components.sync.protocol.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * @deprecated use {@link #getAutofillableLogins(Optional<Account>, Callback<byte[]>,
     *         Callback<Exception>)}.
     */
    default void getAutofillableLogins(
            Callback<byte[]> loginsReply, Callback<Exception> failureCallback){};

    /**
     * Triggers an async list call to retrieve autofillable logins.
     *
     * @param syncingAccount Account used to sync passwords. If the syncingAccount is empty local
     *         account will be used.
     * @param loginsReply Callback that is called on success with serialized {@link
     *         org.chromium.components.sync.protocol.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    default void getAutofillableLogins(Optional<Account> syncingAccount,
            Callback<byte[]> loginsReply, Callback<Exception> failureCallback){};

    /**
     * Triggers an async list call to retrieve logins with matching signon realm.
     *
     * @param signonRealm Signon realm string matched by a substring match. The returned results
     * must be validated (e.g matching "sample.com" also returns logins for "not-sample.com").
     * @param loginsReply Callback that is called on success with serialized {@link
     *         org.chromium.components.sync.protocol.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * @deprecated use {@link #getLoginsForSignonRealm(String, Optional<Account>, Callback<byte[]>,
     *         Callback<Exception>)}.
     */
    default void getLoginsForSignonRealm(String signonRealm, Callback<byte[]> loginsReply,
            Callback<Exception> failureCallback){};

    /**
     * Triggers an async list call to retrieve logins with matching signon realm.
     *
     * @param signonRealm Signon realm string matched by a substring match. The returned results
     * must be validated (e.g matching "sample.com" also returns logins for "not-sample.com").
     * @param syncingAccount Account used to sync passwords. If the syncingAccount is empty local
     *         account will be used.
     * @param loginsReply Callback that is called on success with serialized {@link
     *         org.chromium.components.sync.protocol.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * TODO(crbug.com/1278767): Remove default keyword after downstream implementation is ready.
     */
    default void getLoginsForSignonRealm(String signonRealm, Optional<Account> syncingAccount,
            Callback<byte[]> loginsReply, Callback<Exception> failureCallback){};

    /**
     * Triggers an async call to add a login to the store.
     *
     * @param pwdWithLocalData Serialized PasswordWithLocalData to be added.
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * @deprecated use {@link #addLogin(byte[], Optional<Account>, String, Runnable,
     *         Callback<Exception>)}.
     */
    default void addLogin(byte[] pwdWithLocalData, Runnable successCallback,
            Callback<Exception> failureCallback){};

    /**
     * Triggers an async call to add a login to the store.
     *
     * @param pwdWithLocalData Serialized PasswordWithLocalData to be added.
     * @param syncingAccount Account used to sync passwords. If Nullopt was provided local account
     *         will be used.
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * TODO(crbug.com/1278767): Remove default keyword after downstream implementation is ready.
     */
    default void addLogin(byte[] pwdWithLocalData, Optional<Account> syncingAccount,
            Runnable successCallback, Callback<Exception> failureCallback){};

    /**
     * Triggers an async call to update a login in the store.
     *
     * @param pwdWithLocalData Serialized PasswordWithLocalData replacing a login with the same
     *         unique key.
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * @deprecated use {@link #updateLogin(byte[], Optional<Account>, Runnable,
     *         Callback<Exception>)}.
     */
    default void updateLogin(byte[] pwdWithLocalData, Runnable successCallback,
            Callback<Exception> failureCallback){};

    /**
     * Triggers an async call to update a login in the store.
     *
     * @param pwdWithLocalData Serialized PasswordWithLocalData replacing a login with the same
     *         unique key.
     * @param syncingAccount Account used to sync passwords. If Nullopt was provided local account
     *         will be used.
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * TODO(crbug.com/1278767): Remove default keyword after downstream implementation is ready.
     */
    default void updateLogin(byte[] pwdWithLocalData, Optional<Account> syncingAccount,
            Runnable successCallback, Callback<Exception> failureCallback){};

    /**
     * Triggers an async call to remove a login from store.
     *
     * @param pwdSpecificsData Serialized PasswordSpecificsData identifying the login to be deleted.
     * @param target enum {@link StoreOperationTarget} to decide which storage to use.
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * @deprecated use {@link #removeLogin(byte[], Optional<Account>, Runnable,
     *         Callback<Exception>)}.
     */
    default void removeLogin(byte[] pwdSpecificsData, @PasswordStoreOperationTarget int target,
            Runnable successCallback, Callback<Exception> failureCallback){};

    /**
     * Triggers an async call to remove a login from store.
     *
     * @param pwdSpecificsData Serialized PasswordSpecificsData identifying the login to be deleted.
     * @param syncingAccount Account used to sync passwords. If Nullopt was provided local account
     *         will be used.
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * TODO(crbug.com/1278767): Remove default keyword after downstream implementation is ready.
     */
    default void removeLogin(byte[] pwdSpecificsData, Optional<Account> syncingAccount,
            Runnable successCallback, Callback<Exception> failureCallback){};
}
