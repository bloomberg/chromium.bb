// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The personalization-main component displays the main content of
 * the personalization hub.
 */

import './cros_button_style.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getTemplate} from './personalization_main_element.html.js';
import {isAmbientModeAllowed, Paths, PersonalizationRouter} from './personalization_router_element.js';
import {WithPersonalizationStore} from './personalization_store.js';

export class PersonalizationMain extends WithPersonalizationStore {
  static get is() {
    return 'personalization-main';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      clickable_: {
        type: Boolean,
        value: true,
      },
    };
  }

  private isDarkLightModeEnabled_(): boolean {
    return loadTimeData.getBoolean('isDarkLightModeEnabled');
  }

  private isAmbientModeAllowed_(): boolean {
    return isAmbientModeAllowed();
  }

  private onClickUserSubpageLink_() {
    PersonalizationRouter.instance().goToRoute(Paths.User);
  }

  private onClickAmbientSubpageLink_() {
    PersonalizationRouter.instance().goToRoute(Paths.Ambient);
  }
}

customElements.define(PersonalizationMain.is, PersonalizationMain);
