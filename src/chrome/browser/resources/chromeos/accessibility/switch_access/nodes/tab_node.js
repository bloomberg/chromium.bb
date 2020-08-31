// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles the behavior of tab nodes at the top level (i.e. as
 * groups).
 */
class TabNode extends NodeWrapper {
  /**
   * @param {!chrome.automation.AutomationNode} node The node in the automation
   *    tree
   * @param {?SARootNode} parent
   * @param {!SARootNode} tabAsRoot A pre-calculated object for exploring the
   * parts of tab (i.e. choosing whether to open the tab or close it).
   */
  constructor(node, parent, tabAsRoot) {
    super(node, parent);

    /** @private {!SARootNode} */
    this.tabAsRoot_ = tabAsRoot;
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    return [];
  }

  // ================= General methods =================

  /** @override */
  asRootNode() {
    return this.tabAsRoot_;
  }

  /** @override */
  isGroup() {
    return true;
  }

  // ================= Static methods =================

  /** @override */
  static create(tabNode, parent) {
    const tabAsRoot = new RootNodeWrapper(tabNode);

    let closeButton;
    for (const child of tabNode.children) {
      if (child.role === chrome.automation.RoleType.BUTTON) {
        closeButton = new NodeWrapper(child, tabAsRoot);
        break;
      }
    }
    if (!closeButton) {
      // Pinned tabs have no close button, and so can be treated as just
      // actionable.
      return new ActionableTabNode(tabNode, parent, null);
    }

    const tabToSelect = new ActionableTabNode(tabNode, tabAsRoot, closeButton);
    const backButton = new BackButtonNode(tabAsRoot);
    tabAsRoot.children = [tabToSelect, closeButton, backButton];

    return new TabNode(tabNode, parent, tabAsRoot);
  }
}

/** This class handles the behavior of tabs as actionable elements */
class ActionableTabNode extends NodeWrapper {
  /**
   * @param {!chrome.automation.AutomationNode} node
   * @param {?SARootNode} parent
   * @param {?SAChildNode} closeButton
   */
  constructor(node, parent, closeButton) {
    super(node, parent);

    /** @private {?SAChildNode} */
    this.closeButton_ = closeButton;
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    return [SAConstants.MenuAction.SELECT];
  }

  /** @override */
  get location() {
    if (!this.closeButton_) {
      return super.location;
    }
    return RectHelper.difference(super.location, this.closeButton_.location);
  }

  // ================= General methods =================

  /** @override */
  asRootNode() {
    return null;
  }

  /** @override */
  isGroup() {
    return false;
  }
}
