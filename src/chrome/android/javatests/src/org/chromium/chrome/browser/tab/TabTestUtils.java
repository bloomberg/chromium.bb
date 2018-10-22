// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;

/**
 * Exposes helper functions to be used in tests to instrument tab interaction.
 */
public class TabTestUtils {

    /**
     * @return The observers registered for the given tab.
     */
    public static ObserverList.RewindableIterator<TabObserver> getTabObservers(Tab tab) {
        return tab.getTabObservers();
    }

    /**
     * Simulates the first visually non empty paint for the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     */
    public static void simulateFirstVisuallyNonEmptyPaint(Tab tab) {
        RewindableIterator<TabObserver> observers = tab.getTabObservers();
        while (observers.hasNext()) observers.next().didFirstVisuallyNonEmptyPaint(tab);
    }

    /**
     * Simulates page loaded for the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     */
    public static void simulatePageLoadFinished(Tab tab) {
        RewindableIterator<TabObserver> observers = tab.getTabObservers();
        while (observers.hasNext()) observers.next().onPageLoadFinished(tab);
    }

    /**
     * Simulates page load failed for the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     * @param errorCode Errorcode to send to the page.
     */
    public static void simulatePageLoadFailed(Tab tab, int errorCode) {
        RewindableIterator<TabObserver> observers = tab.getTabObservers();
        while (observers.hasNext()) observers.next().onPageLoadFailed(tab, errorCode);
    }

    /**
     * Simulates a crash of the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     * @param sadTabShown Whether the sad tab was shown.
     */
    public static void simulateCrash(Tab tab, boolean sadTabShown) {
        RewindableIterator<TabObserver> observers = tab.getTabObservers();
        while (observers.hasNext()) observers.next().onCrash(tab, sadTabShown);
    }

    /**
     * Simulates a change of theme color for the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     * @param color Color to send to the tab.
     */
    public static void simulateChangeThemeColor(Tab tab, int color) {
        RewindableIterator<TabObserver> observers = tab.getTabObservers();
        while (observers.hasNext()) observers.next().onDidChangeThemeColor(tab, color);
    }
}
