// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Handler;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

/**
 * Unit tests for {@link ChromeMessageQueueMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class ChromeMessageQueueMediatorTest {
    private static final int EXPECTED_TOKEN = 42;

    @Mock
    private BrowserControlsManager mBrowserControlsManager;

    @Mock
    private MessageContainerCoordinator mMessageContainerCoordinator;

    @Mock
    private LayoutStateProvider mLayoutStateProvider;

    @Mock
    private ManagedMessageDispatcher mMessageDispatcher;

    @Mock
    private ModalDialogManager mModalDialogManager;

    @Mock
    private ActivityTabProvider mActivityTabProvider;

    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    @Mock
    private Handler mQueueHandler;

    private ChromeMessageQueueMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMessageDispatcher.suspend()).thenReturn(EXPECTED_TOKEN);
    }

    private void initMediator() {
        OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderOneShotSupplier =
                new OneshotSupplierImpl<>();
        ObservableSupplierImpl<ModalDialogManager> modalDialogManagerSupplier =
                new ObservableSupplierImpl<>();
        mMediator = new ChromeMessageQueueMediator(mBrowserControlsManager,
                mMessageContainerCoordinator, mActivityTabProvider,
                layoutStateProviderOneShotSupplier, modalDialogManagerSupplier,
                mActivityLifecycleDispatcher, mMessageDispatcher);
        layoutStateProviderOneShotSupplier.set(mLayoutStateProvider);
        modalDialogManagerSupplier.set(mModalDialogManager);
        mMediator.setQueueHandlerForTesting(mQueueHandler);
    }

    /**
     * Test the queue can be suspended and resumed correctly when toggling layout state change.
     */
    @Test
    public void testLayoutStateChange() {
        final ArgumentCaptor<LayoutStateObserver> observer =
                ArgumentCaptor.forClass(LayoutStateObserver.class);
        doNothing().when(mLayoutStateProvider).addObserver(observer.capture());
        initMediator();
        observer.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        verify(mMessageDispatcher).suspend();
        observer.getValue().onFinishedShowing(LayoutType.BROWSING);
        verify(mMessageDispatcher).resume(EXPECTED_TOKEN);
    }

    /**
     * Test the queue can be suspended and resumed correctly when showing/hiding modal dialogs.
     */
    @Test
    public void testModalDialogChange() {
        final ArgumentCaptor<ModalDialogManagerObserver> observer =
                ArgumentCaptor.forClass(ModalDialogManagerObserver.class);
        doNothing().when(mModalDialogManager).addObserver(observer.capture());
        initMediator();
        observer.getValue().onDialogAdded(new PropertyModel());
        verify(mMessageDispatcher).suspend();
        observer.getValue().onLastDialogDismissed();
        verify(mMessageDispatcher).resume(EXPECTED_TOKEN);
    }

    /**
     * Test the queue can be suspended and resumed correctly when app is paused and resumed.
     */
    @Test
    public void testActivityStateChange() {
        final ArgumentCaptor<PauseResumeWithNativeObserver> observer =
                ArgumentCaptor.forClass(PauseResumeWithNativeObserver.class);
        doNothing().when(mActivityLifecycleDispatcher).register(observer.capture());
        initMediator();
        observer.getValue().onPauseWithNative();
        verify(mMessageDispatcher).suspend();
        observer.getValue().onResumeWithNative();
        verify(mMessageDispatcher).resume(EXPECTED_TOKEN);
    }

    /**
     * Test the runnable by #onStartShow is reset correctly.
     */
    @Test
    public void testResetOnStartShowRunnable() {
        when(mBrowserControlsManager.getBrowserControlHiddenRatio()).thenReturn(0.5f);
        OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderOneShotSupplier =
                new OneshotSupplierImpl<>();
        ObservableSupplierImpl<ModalDialogManager> modalDialogManagerSupplier =
                new ObservableSupplierImpl<>();
        final ArgumentCaptor<ChromeMessageQueueMediator.BrowserControlsObserver>
                observerArgumentCaptor = ArgumentCaptor.forClass(
                        ChromeMessageQueueMediator.BrowserControlsObserver.class);
        doNothing().when(mBrowserControlsManager).addObserver(observerArgumentCaptor.capture());
        when(mBrowserControlsManager.getBrowserVisibilityDelegate())
                .thenReturn(new BrowserStateBrowserControlsVisibilityDelegate(
                        new ObservableSupplier<Boolean>() {
                            @Override
                            public Boolean addObserver(Callback<Boolean> obs) {
                                return null;
                            }

                            @Override
                            public void removeObserver(Callback<Boolean> obs) {}

                            @Override
                            public Boolean get() {
                                return false;
                            }
                        }));
        mMediator = new ChromeMessageQueueMediator(mBrowserControlsManager,
                mMessageContainerCoordinator, mActivityTabProvider,
                layoutStateProviderOneShotSupplier, modalDialogManagerSupplier,
                mActivityLifecycleDispatcher, mMessageDispatcher);
        ChromeMessageQueueMediator.BrowserControlsObserver observer =
                observerArgumentCaptor.getValue();
        Runnable runnable = () -> {};
        mMediator.onStartShowing(runnable);
        Assert.assertNotNull(observer.getRunnableForTesting());

        mMediator.onFinishHiding();
        Assert.assertNull("Callback should be reset to null after hiding is finished",
                observer.getRunnableForTesting());
    }

    /**
     * Test NPE is not thrown when supplier offers a null value.
     */
    @Test
    public void testThrowNothingWhenModalDialogManagerIsNull() {
        OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderOneShotSupplier =
                new OneshotSupplierImpl<>();
        ObservableSupplierImpl<ModalDialogManager> modalDialogManagerSupplier =
                new ObservableSupplierImpl<>();
        mMediator = new ChromeMessageQueueMediator(mBrowserControlsManager,
                mMessageContainerCoordinator, mActivityTabProvider,
                layoutStateProviderOneShotSupplier, modalDialogManagerSupplier,
                mActivityLifecycleDispatcher, mMessageDispatcher);
        layoutStateProviderOneShotSupplier.set(mLayoutStateProvider);
        // To offer a null value, we have to offer a value other than null first.
        modalDialogManagerSupplier.set(mModalDialogManager);
        modalDialogManagerSupplier.set(null);
    }

    /**
     * Test the queue can be suspended and resumed correctly on omnibox focus events.
     */
    @Test
    public void testUrlFocusChange() {
        initMediator();
        // Omnibox is focused.
        mMediator.onUrlFocusChange(true);
        verify(mMessageDispatcher).suspend();
        verify(mQueueHandler).removeCallbacksAndMessages(null);
        // Omnibox is out of focus.
        mMediator.onUrlFocusChange(false);
        ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        // Verify that the queue is resumed 1s after the omnibox loses focus.
        verify(mQueueHandler).postDelayed(captor.capture(), eq(1000L));
        captor.getValue().run();
        verify(mMessageDispatcher).resume(EXPECTED_TOKEN);
        Assert.assertEquals("mUrlFocusToken should be invalidated.", TokenHolder.INVALID_TOKEN,
                mMediator.getUrlFocusTokenForTesting());
    }
}
