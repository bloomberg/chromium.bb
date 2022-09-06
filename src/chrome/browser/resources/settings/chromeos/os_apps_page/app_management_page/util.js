// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router} from '../../../router.js';
import {routes} from '../../os_route.js';

/**
 * Navigates to the App Detail page.
 *
 * @param {string} appId
 */
export function openAppDetailPage(appId) {
  const params = new URLSearchParams();
  params.append('id', appId);
  Router.getInstance().navigateTo(routes.APP_MANAGEMENT_DETAIL, params);
}

/**
 * Navigates to the main App Management list page.
 */
export function openMainPage() {
  Router.getInstance().navigateTo(routes.APP_MANAGEMENT);
}
