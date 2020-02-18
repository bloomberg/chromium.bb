// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

/**
 * Interface used by NavigationController to inform the client of changes. This largely duplicates
 * the NavigationObserver interface, but is a singleton to avoid unnecessary IPC.
 */
interface INavigationControllerClient {
  IClientNavigation createClientNavigation(in INavigation impl) = 0;

  void navigationStarted(IClientNavigation navigation) = 1;

  void navigationRedirected(IClientNavigation navigation) = 2;

  void navigationCommitted(IClientNavigation navigation) = 3;

  void navigationCompleted(IClientNavigation navigation) = 4;

  void navigationFailed(IClientNavigation navigation) = 5;
}
