// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.messages.MessageScopeChange.ChangeType;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.mock.MockWebContents;

/**
 * A test for {@link ScopeChangeController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ScopeChangeControllerTest {
    private static final boolean IS_MAIN_FRAME = true;
    private static final boolean IS_SAME_DOCUMENT = true;
    private static final boolean IS_RELOAD = true;
    private static final boolean DID_COMMIT = true;

    @Test
    @SmallTest
    public void testNavigationScopeChange() {
        ScopeChangeController.Delegate delegate =
                Mockito.mock(ScopeChangeController.Delegate.class);
        ScopeChangeController controller = new ScopeChangeController(delegate);

        MockWebContents webContents = mock(MockWebContents.class);

        int expectedOnScopeChangeCalls = 0;
        ScopeKey key = new ScopeKey(MessageScopeType.NAVIGATION, webContents);
        controller.firstMessageEnqueued(key);

        final ArgumentCaptor<WebContentsObserver> runnableCaptor =
                ArgumentCaptor.forClass(WebContentsObserver.class);
        verify(webContents).addObserver(runnableCaptor.capture());

        WebContentsObserver observer = runnableCaptor.getValue();

        // Default visibility of web contents is invisible.
        expectedOnScopeChangeCalls++;
        ArgumentCaptor<MessageScopeChange> captor =
                ArgumentCaptor.forClass(MessageScopeChange.class);
        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description("Delegate should be called when page is hidden"))
                .onScopeChange(captor.capture());
        Assert.assertEquals("Scope type should be inactive when page is hidden",
                ChangeType.INACTIVE, captor.getValue().changeType);

        observer.onWebContentsFocused();
        expectedOnScopeChangeCalls++;

        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description("Delegate should be called when page is shown"))
                .onScopeChange(captor.capture());
        Assert.assertEquals("Scope type should be active when page is shown", ChangeType.ACTIVE,
                captor.getValue().changeType);

        observer.onWebContentsLostFocus();
        expectedOnScopeChangeCalls++;
        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description("Delegate should be called when page is hidden"))
                .onScopeChange(captor.capture());
        Assert.assertEquals("Scope type should be inactive when page is hidden",
                ChangeType.INACTIVE, captor.getValue().changeType);

        observer.didFinishNavigation(
                createNavigationHandle(IS_MAIN_FRAME, !IS_SAME_DOCUMENT, IS_RELOAD, DID_COMMIT));
        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description("Delegate should not be called for a refresh"))
                .onScopeChange(any());

        observer.didFinishNavigation(
                createNavigationHandle(!IS_MAIN_FRAME, !IS_SAME_DOCUMENT, !IS_RELOAD, DID_COMMIT));
        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description("Delegate should not be called for a subframe"))
                .onScopeChange(any());

        observer.didFinishNavigation(
                createNavigationHandle(IS_MAIN_FRAME, !IS_SAME_DOCUMENT, !IS_RELOAD, !DID_COMMIT));
        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description("Delegate should not be called for uncommitted navigations"))
                .onScopeChange(any());

        observer.didFinishNavigation(
                createNavigationHandle(IS_MAIN_FRAME, IS_SAME_DOCUMENT, !IS_RELOAD, DID_COMMIT));
        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description("Delegate should not be called for same document navigations"))
                .onScopeChange(any());

        observer.didFinishNavigation(
                createNavigationHandle(IS_MAIN_FRAME, !IS_SAME_DOCUMENT, !IS_RELOAD, DID_COMMIT));
        expectedOnScopeChangeCalls++;
        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description(
                                "Delegate should be called when page is navigated to another page"))
                .onScopeChange(captor.capture());
        Assert.assertEquals("Scope type should be destroy when navigated to another page",
                ChangeType.DESTROY, captor.getValue().changeType);

        observer.onTopLevelNativeWindowChanged(null);
        expectedOnScopeChangeCalls++;
        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description(
                                "Delegate should be called when top level native window changes"))
                .onScopeChange(captor.capture());
        Assert.assertEquals("Scope type should be destroy when top level native window changes",
                ChangeType.DESTROY, captor.getValue().changeType);
    }

    @Test
    @SmallTest
    public void testIgnoreNavigation() {
        ScopeChangeController.Delegate delegate =
                Mockito.mock(ScopeChangeController.Delegate.class);
        ScopeChangeController controller = new ScopeChangeController(delegate);

        MockWebContents webContents = mock(MockWebContents.class);
        ScopeKey key = new ScopeKey(MessageScopeType.WEB_CONTENTS, webContents);
        controller.firstMessageEnqueued(key);

        final ArgumentCaptor<WebContentsObserver> runnableCaptor =
                ArgumentCaptor.forClass(WebContentsObserver.class);
        verify(webContents).addObserver(runnableCaptor.capture());

        WebContentsObserver observer = runnableCaptor.getValue();

        // Default visibility of web contents is invisible.
        ArgumentCaptor<MessageScopeChange> captor =
                ArgumentCaptor.forClass(MessageScopeChange.class);
        verify(delegate, description("Delegate should be called when page is hidden"))
                .onScopeChange(captor.capture());
        Assert.assertEquals("Scope type should be inactive when page is hidden",
                ChangeType.INACTIVE, captor.getValue().changeType);

        observer.didFinishNavigation(
                createNavigationHandle(IS_MAIN_FRAME, !IS_SAME_DOCUMENT, !IS_RELOAD, DID_COMMIT));
        verify(delegate,
                times(1).description("Delegate should not be called when navigation is ignored"))
                .onScopeChange(any());
    }

    private NavigationHandle createNavigationHandle(
            boolean isMainFrame, boolean isSameDocument, boolean isReload, boolean didCommit) {
        NavigationHandle handle = new NavigationHandle(0, null, null, null, isMainFrame,
                isSameDocument, true, null, 0, false, false, false, false, -1, false, isReload);
        handle.didFinish(null, false, didCommit, false, false, false, 0, 0, 0);
        return handle;
    }
}
