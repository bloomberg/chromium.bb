// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as SDK from '../sdk/sdk.js';
import * as UI from '../ui/ui.js';

import {AccessibilityTree} from './AccessibilityTree.js';
import {sdkNodeToAXNode} from './AccessibilityTreeUtils.js';

// This class simply acts as a wrapper around the AccessibilityTree web component for
// compatibility with DevTools legacy UI widgets. It in itself contains no business logic
// or functionality.
export class AccessibilityTreeView extends UI.Widget.VBox {
  private readonly accessibilityTreeComponent = new AccessibilityTree();
  private readonly toggleButton: HTMLButtonElement;

  constructor(toggleButton: HTMLButtonElement) {
    super();
    // toggleButton is bound to a click handler on ElementsPanel to switch between the DOM tree
    // and accessibility tree views.
    this.toggleButton = toggleButton;
    this.contentElement.appendChild(this.toggleButton);
    this.contentElement.appendChild(this.accessibilityTreeComponent);
  }

  async refreshAccessibilityTree(node: SDK.DOMModel.DOMNode): Promise<SDK.AccessibilityModel.AccessibilityNode|null> {
    const accessibilityModel = node.domModel().target().model(SDK.AccessibilityModel.AccessibilityModel);
    if (!accessibilityModel) {
      return null;
    }

    const result = await accessibilityModel.requestRootNode();
    return result || null;
  }

  async setNode(inspectedNode: SDK.DOMModel.DOMNode): Promise<void> {
    const root = await this.refreshAccessibilityTree(inspectedNode);
    if (!root) {
      return;
    }
    this.accessibilityTreeComponent.data = {
      rootNode: sdkNodeToAXNode(null, root, this.accessibilityTreeComponent),
    };
  }
}
