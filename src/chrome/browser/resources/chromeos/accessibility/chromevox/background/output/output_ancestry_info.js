// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a class which computes various types of ancestor
 * chains given the current node.
 */

goog.provide('OutputAncestryInfo');

goog.require('constants');
goog.require('OutputRoleInfo');

goog.scope(function() {
const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const RoleType = chrome.automation.RoleType;

/**
 * This class computes four types of ancestry chains (see below for specific
 * definition). By default, enter and leave are computed, with an option to
 * compute start and end ancestors on construction. These ancestors are cached,
 * so are generally valid for the current call stack, wherein ancestry data is
 * stable.
 */
OutputAncestryInfo = class {
  /**
   * @param {!AutomationNode} node The primary node to consider for ancestry
   *     computation.
   * @param {!AutomationNode} prevNode The previous node (in user-initiated
   *     navigation).
   * @param {boolean} suppressStartAndEndAncestors Whether to compute |node|'s
   *     start and end ancestors (see below for definitions).
   */
  constructor(node, prevNode, suppressStartAndEndAncestors = true) {
    /** @private {!Array<!AutomationNode>} */
    this.enterAncestors_ = OutputAncestryInfo.byContextFirst_(
        AutomationUtil.getUniqueAncestors(prevNode, node));
    /** @private {!Array<!AutomationNode>} */
    this.leaveAncestors_ =
        OutputAncestryInfo
            .byContextFirst_(AutomationUtil.getUniqueAncestors(node, prevNode))
            .reverse();
    /** @private {!Array<!AutomationNode>} */
    this.endAncestors_ = [];
    /** @private {!Array<!AutomationNode>} */
    this.startAncestors_ = [];

    if (suppressStartAndEndAncestors) {
      return;
    }

    let afterEndNode = AutomationUtil.findNextNode(
        node, Dir.FORWARD, AutomationPredicate.leafOrStaticText,
        {skipInitialSubtree: true});
    if (!afterEndNode) {
      afterEndNode = AutomationUtil.getTopLevelRoot(node) || node.root;
    }

    if (afterEndNode) {
      this.endAncestors_ = OutputAncestryInfo.byContextFirst_(
          AutomationUtil.getUniqueAncestors(afterEndNode, node));
    }

    let beforeStartNode = AutomationUtil.findNextNode(
        node, Dir.BACKWARD, AutomationPredicate.leafOrStaticText,
        {skipInitialAncestry: true});
    if (!beforeStartNode) {
      beforeStartNode = AutomationUtil.getTopLevelRoot(node) || node.root;
    }

    if (beforeStartNode) {
      this.startAncestors_ =
          OutputAncestryInfo
              .byContextFirst_(
                  AutomationUtil.getUniqueAncestors(beforeStartNode, node))
              .reverse();
    }
  }

  /**
   * Enter ancestors are all ancestors of |node| which are not ancestors of
   * |prevNode|. This list of nodes is useful when trying to understand what
   * nodes to consider for a newly positioned node e.g. for speaking focus. One
   * can think of this as a leave ancestry list if we swap |ndoe| and
   * |prevNode|.
   * @return {!Array<!AutomationNode>}
   */
  get enterAncestors() {
    return this.enterAncestors_;
  }

  /**
   * Leave ancestors are all ancestors of |prevNode| which are not ancestors of
   * |node|. This list of nodes is useful when trying to understand what nodes
   * to consider for a newly unpositioned node e.g. for speaking blur. One can
   * think of this as an enter ancestry list if we swap |ndoe| and |prevNode|.
   * @return {!Array<!AutomationNode>}
   */
  get leaveAncestors() {
    return this.leaveAncestors_;
  }

  /**
   * Start ancestors are all ancestors of |node| when which are not ancestors of
   * the node immediately before |node| in linear object navigation. This list
   * of nodes is useful when trying to understand what nodes to consider for
   * describing |node|'s ancestry context limited to ancestors of most
   * relevance.
   * @return {!Array<!AutomationNode>}
   */
  get startAncestors() {
    return this.startAncestors_;
  }

  /**
   * End ancestors are all ancestors of |node| which are not ancestors of the
   * node immediately after |node| in linear object navigation. This list of
   * nodes is useful when trying to understand what nodes to consider for
   * describing |node|'s ancestry context limited to ancestors of most
   * relevance.
   * @return {!Array<!AutomationNode>}
   */
  get endAncestors() {
    return this.endAncestors_;
  }

  /**
   * @param {!Array<!AutomationNode>} ancestors
   * @return {!Array<!AutomationNode>}
   * @private
   */
  static byContextFirst_(ancestors) {
    let contextFirst = [];
    let rest = [];
    for (let i = 0; i < ancestors.length - 1; i++) {
      const node = ancestors[i];
      // Discard ancestors of deepest window.
      if (node.role === RoleType.WINDOW) {
        contextFirst = [];
        rest = [];
      }
      if ((OutputRoleInfo[node.role] || {}).contextOrder ===
          OutputContextOrder.FIRST) {
        contextFirst.push(node);
      } else {
        rest.push(node);
      }
    }
    return rest.concat(contextFirst.reverse());
  }
};
});  // goog.scope
