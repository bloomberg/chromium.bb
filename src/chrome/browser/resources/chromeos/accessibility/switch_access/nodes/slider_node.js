// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SAConstants, SwitchAccessMenuAction} from '../switch_access_constants.js';

import {BasicNode} from './basic_node.js';
import {SAChildNode, SARootNode} from './switch_access_node.js';

const AutomationNode = chrome.automation.AutomationNode;

/** This class handles interactions with sliders. */
export class SliderNode extends BasicNode {
  /**
   * @param {!AutomationNode} baseNode
   * @param {?SARootNode} parent
   */
  constructor(baseNode, parent) {
    super(baseNode, parent);
    this.isCustomSlider_ = !!baseNode.htmlAttributes.role;
  }

  /** @override */
  onFocus() {
    super.onFocus();
    this.automationNode.focus();
  }

  /** @override */
  performAction(action) {
    // Currently, custom sliders have no way to support increment/decrement via
    // the automation API. We handle this case by simulating left/right arrow
    // presses.
    if (this.isCustomSlider_) {
      if (action === SwitchAccessMenuAction.INCREMENT) {
        EventGenerator.sendKeyPress(KeyCode.RIGHT);
        return SAConstants.ActionResponse.REMAIN_OPEN;
      } else if (action === SwitchAccessMenuAction.DECREMENT) {
        EventGenerator.sendKeyPress(KeyCode.LEFT);
        return SAConstants.ActionResponse.REMAIN_OPEN;
      }
    }

    return super.performAction(action);
  }
}

BasicNode.creators.push({
  predicate: baseNode => baseNode.role === chrome.automation.RoleType.SLIDER,
  creator: (node, parent) => new SliderNode(node, parent)
});
