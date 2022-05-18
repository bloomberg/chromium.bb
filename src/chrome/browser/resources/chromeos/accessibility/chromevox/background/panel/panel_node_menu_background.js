// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Calculates the menu items for the node menus in the ChromeVox
 * panel.
 */

const AutomationNode = chrome.automation.AutomationNode;

export class PanelNodeMenuBackground {
  /**
   * @param {!PanelNodeMenuData} menuData
   * @param {AutomationNode} node ChromeVox's current position.
   * @param {boolean} isActivated Whether the menu was explicitly activated.
   *     If false, the menu is populated asynchronously by posting a task
   *     after searching each chunk of nodes.
   */
  constructor(menuData, node, isActivated) {
    /** @private {AutomationNode} */
    this.node_ = node;
    /** @private {AutomationPredicate.Unary} */
    this.pred_ = menuData.predicate;
    /** @private {!PanelNodeMenuId} */
    this.menuId_ = menuData.menuId;
    /** @private {boolean} */
    this.isActivated_ = isActivated;
    /** @private {AutomationTreeWalker|undefined} */
    this.walker_;
    /** @private {number} */
    this.nodeCount_ = 0;
    /** @private {boolean} */
    this.isEmpty_ = true;
  }

  /** @param {number} callbackNodeIndex */
  static focusNodeCallback(callbackNodeIndex) {
    if (callbackNodeIndex < 0 ||
        callbackNodeIndex >= PanelNodeMenuBackground.callbackNodes_.length) {
      return;
    }
    ChromeVoxState.instance.navigateToRange(cursors.Range.fromNode(
        PanelNodeMenuBackground.callbackNodes_[callbackNodeIndex]));
  }

  /**
   * Create the AutomationTreeWalker and kick off the search to find
   * nodes that match the predicate for this menu.
   */
  populate() {
    if (!this.node_) {
      this.finish_();
      return;
    }

    const root = AutomationUtil.getTopLevelRoot(this.node_);
    if (!root) {
      this.finish_();
      return;
    }

    this.walker_ = new AutomationTreeWalker(root, constants.Dir.FORWARD, {
      visit(node) {
        return !AutomationPredicate.shouldIgnoreNode(node);
      }
    });
    this.nodeCount_ = 0;
    this.findMoreNodes_();
  }

  /**
   * Iterate over nodes from the tree walker. If a node matches the
   * predicate, add an item to the menu.
   *
   * Unless |this.isActivated_| is true, then after MAX_NODES_BEFORE_ASYNC nodes
   * have been scanned, call setTimeout to defer searching. This frees up the
   * main event loop to keep the panel menu responsive, otherwise it basically
   * freezes up until all of the nodes have been found.
   * @private
   */
  findMoreNodes_() {
    while (this.walker_.next().node) {
      const node = this.walker_.node;
      if (this.pred_(node)) {
        this.isEmpty_ = false;
        const output = new Output();
        const range = cursors.Range.fromNode(node);
        output.withoutHints();
        output.withSpeech(range, range, OutputEventType.NAVIGATE);
        const title = output.toString();

        const callbackNodeIndex = PanelNodeMenuBackground.callbackNodes_.length;
        PanelNodeMenuBackground.callbackNodes_.push(node);
        const isActive = node === this.node_ && this.isActivated_;
        const menuId = this.menuId_;
        this.addMenuItemFromData_({title, callbackNodeIndex, isActive, menuId});
      }

      if (!this.isActivated_) {
        this.nodeCount_++;
        if (this.nodeCount_ >= PanelNodeMenuBackground.MAX_NODES_BEFORE_ASYNC) {
          this.nodeCount_ = 0;
          window.setTimeout(this.findMoreNodes_.bind(this), 0);
          return;
        }
      }
    }
    this.finish_();
  }

  /**
   * Called when we've finished searching for nodes. If no matches were
   * found, adds an item to the menu indicating none were found.
   * @private
   */
  finish_() {
    if (this.isEmpty_) {
      this.addMenuItemFromData_({
        title: Msgs.getMsg('panel_menu_item_none'),
        callbackNodeIndex: -1,
        isActive: false,
        menuId: this.menuId_,
      });
    }
  }

  /**
   * @param {!PanelNodeMenuItemData} itemData
   * @private
   */
  async addMenuItemFromData_(itemData) {
    await PanelBridge.addMenuItem(itemData);
  }
}

/**
 * The number of nodes to search before posting a task to finish
 * searching.
 * @const {number}
 */
PanelNodeMenuBackground.MAX_NODES_BEFORE_ASYNC = 100;

/**
 * An array of nodes associated with a PanelNodeMenuItem, saved so we can
 * set the current ChromeVox range when the user selects an item.
 * @private {!Array<chrome.automation.AutomationNode>}
 */
PanelNodeMenuBackground.callbackNodes_ = [];
