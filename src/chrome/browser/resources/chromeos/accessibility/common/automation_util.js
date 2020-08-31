// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox utilities for the automation extension API.
 */

goog.provide('AutomationUtil');

goog.require('AutomationPredicate');
goog.require('AutomationTreeWalker');
goog.require('constants');

goog.scope(function() {
const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const RoleType = chrome.automation.RoleType;

AutomationUtil = class {
  constructor() {}

  /**
   * Find a node in subtree of |cur| satisfying |pred| using pre-order
   * traversal.
   * @param {AutomationNode} cur Node to begin the search
   *     from.
   * @param {Dir} dir
   * @param {AutomationPredicate.Unary} pred A predicate to apply
   *     to a candidate node.
   * @return {AutomationNode}
   */
  static findNodePre(cur, dir, pred) {
    if (!cur) {
      return null;
    }

    if (pred(cur) && !AutomationPredicate.shouldIgnoreNode(cur)) {
      return cur;
    }

    let child = dir == Dir.BACKWARD ? cur.lastChild : cur.firstChild;
    while (child) {
      const ret = AutomationUtil.findNodePre(child, dir, pred);
      if (ret) {
        return ret;
      }
      child = dir == Dir.BACKWARD ? child.previousSibling : child.nextSibling;
    }
    return null;
  }

  /**
   * Find a node in subtree of |cur| satisfying |pred| using post-order
   * traversal.
   * @param {AutomationNode} cur Node to begin the search
   *     from.
   * @param {Dir} dir
   * @param {AutomationPredicate.Unary} pred A predicate to apply
   *     to a candidate node.
   * @return {AutomationNode}
   */
  static findNodePost(cur, dir, pred) {
    if (!cur) {
      return null;
    }

    let child = dir == Dir.BACKWARD ? cur.lastChild : cur.firstChild;
    while (child) {
      const ret = AutomationUtil.findNodePost(child, dir, pred);
      if (ret) {
        return ret;
      }
      child = dir == Dir.BACKWARD ? child.previousSibling : child.nextSibling;
    }

    if (pred(cur) && !AutomationPredicate.shouldIgnoreNode(cur)) {
      return cur;
    }

    return null;
  }

  /**
   * Find the next node in the given direction in depth first order.
   *
   * Let D be the dfs linearization of |cur.root|. Then, let F be the list after
   * applying |pred| as a filter to D. This method will return the directed next
   * node of |cur| in F.
   * The restrictions option will further filter F. For example,
   * |skipInitialSubtree| will remove any |pred| matches in the subtree of |cur|
   * from F.
   * @param {!AutomationNode} cur Node to begin the search
   *     from.
   * @param {Dir} dir
   * @param {AutomationPredicate.Unary} pred A predicate to apply
   *     to a candidate node.
   * @param {AutomationTreeWalkerRestriction=} opt_restrictions |leaf|, |root|,
   *     |skipInitialAncestry|, and |skipInitialSubtree| are valid restrictions
   *     used when finding the next node.
   *     By default:
   *        the root predicate ges set to |AutomationPredicate.root|.
   *        |skipInitialSubtree| is false if |cur| is a container or matches
   *        |pred|. This alleviates the caller from syncing forwards.
   *        Leaves are nodes matched by |prred| which are not also containers.
   *        This takes care of syncing backwards.
   * @return {AutomationNode}
   */
  static findNextNode(cur, dir, pred, opt_restrictions) {
    const restrictions = {};
    opt_restrictions = opt_restrictions || {
      leaf: undefined,
      root: undefined,
      visit: undefined,
      skipInitialSubtree: !AutomationPredicate.container(cur) && pred(cur)
    };

    restrictions.root = opt_restrictions.root || AutomationPredicate.root;
    restrictions.leaf = opt_restrictions.leaf || function(node) {
      // Treat nodes matched by |pred| as leaves except for containers.
      return !AutomationPredicate.container(node) && pred(node);
    };

    restrictions.skipInitialSubtree = opt_restrictions.skipInitialSubtree;
    restrictions.skipInitialAncestry = opt_restrictions.skipInitialAncestry;

    restrictions.visit = function(node) {
      return pred(node) && !AutomationPredicate.shouldIgnoreNode(node);
    };

    const walker = new AutomationTreeWalker(cur, dir, restrictions);
    return walker.next().node;
  }

  /**
   * Given nodes a_1, ..., a_n starting at |cur| in pre order traversal, apply
   * |pred| to a_i and a_(i - 1) until |pred| is satisfied.  Returns a_(i - 1)
   * or a_i (depending on opt_before) or null if no match was found.
   * @param {!AutomationNode} cur
   * @param {Dir} dir
   * @param {AutomationPredicate.Binary} pred
   * @param {boolean=} opt_before True to return a_(i - 1); a_i otherwise.
   *                              Defaults to false.
   * @return {AutomationNode}
   */
  static findNodeUntil(cur, dir, pred, opt_before) {
    let before = cur;
    let after = before;
    do {
      before = after;
      after =
          AutomationUtil.findNextNode(before, dir, AutomationPredicate.leaf);
    } while (after && !pred(before, after));
    return opt_before ? before : after;
  }

  /**
   * Returns an array containing ancestors of node starting at root down to
   * node.
   * @param {!AutomationNode} node
   * @return {!Array<AutomationNode>}
   */
  static getAncestors(node) {
    const ret = [];
    let candidate = node;
    while (candidate) {
      ret.push(candidate);

      candidate = candidate.parent;
    }
    return ret.reverse();
  }

  /**
   * Gets the first index where the two input arrays differ. Returns -1 if they
   * do not.
   * @param {!Array<AutomationNode>} ancestorsA
   * @param {!Array<AutomationNode>} ancestorsB
   * @return {number}
   */
  static getDivergence(ancestorsA, ancestorsB) {
    for (let i = 0; i < ancestorsA.length; i++) {
      if (ancestorsA[i] !== ancestorsB[i]) {
        return i;
      }
    }
    if (ancestorsA.length == ancestorsB.length) {
      return -1;
    }
    return ancestorsA.length;
  }

  /**
   * Returns ancestors of |node| that are not also ancestors of |prevNode|.
   * @param {!AutomationNode} prevNode
   * @param {!AutomationNode} node
   * @return {!Array<!AutomationNode>}
   */
  static getUniqueAncestors(prevNode, node) {
    const prevAncestors = AutomationUtil.getAncestors(prevNode);
    const ancestors = AutomationUtil.getAncestors(node);
    const divergence = AutomationUtil.getDivergence(prevAncestors, ancestors);
    return ancestors.slice(divergence);
  }

  /**
   * Given |nodeA| and |nodeB| in that order, determines their ordering in the
   * document.
   * @param {!AutomationNode} nodeA
   * @param {!AutomationNode} nodeB
   * @return {Dir}
   */
  static getDirection(nodeA, nodeB) {
    const ancestorsA = AutomationUtil.getAncestors(nodeA);
    const ancestorsB = AutomationUtil.getAncestors(nodeB);
    const divergence = AutomationUtil.getDivergence(ancestorsA, ancestorsB);

    // Default to Dir.FORWARD.
    if (divergence == -1) {
      return Dir.FORWARD;
    }

    const divA = ancestorsA[divergence];
    const divB = ancestorsB[divergence];

    // One of the nodes is an ancestor of the other. Order this relationship in
    // the same way dfs would. nodeA <= nodeB if nodeA is a descendant of
    // nodeB. nodeA > nodeB if nodeB is a descendant of nodeA.

    if (!divA) {
      return Dir.FORWARD;
    }
    if (!divB) {
      return Dir.BACKWARD;
    }
    if (divA.parent === nodeB) {
      return Dir.BACKWARD;
    }
    if (divB.parent === nodeA) {
      return Dir.FORWARD;
    }

    return divA.indexInParent <= divB.indexInParent ? Dir.FORWARD :
                                                      Dir.BACKWARD;
  }

  /**
   * Determines whether the two given nodes come from the same tree source.
   * @param {AutomationNode} a
   * @param {AutomationNode} b
   * @return {boolean}
   */
  static isInSameTree(a, b) {
    if (!a || !b) {
      return true;
    }

    // Given two non-desktop roots, consider them in the "same" tree.
    return a.root === b.root ||
        (a.root.role == b.root.role && a.root.role == RoleType.ROOT_WEB_AREA);
  }

  /**
   * Determines whether or not a node is or is the descendant of another node.
   * @param {!AutomationNode} node
   * @param {!AutomationNode} ancestor
   * @return {boolean}
   */
  static isDescendantOf(node, ancestor) {
    let testNode = node;
    while (testNode && testNode !== ancestor) {
      testNode = testNode.parent;
    }
    return testNode === ancestor;
  }

  /**
   * Finds the deepest node containing point. Since the automation tree does not
   * maintain a containment invariant when considering child node bounding rects
   * with respect to their parents, the hit test considers all children before
   * their parents when looking for a matching node.
   * @param {AutomationNode} node Subtree to search.
   * @param {constants.Point} point
   * @return {AutomationNode}
   */
  static hitTest(node, point) {
    let child = node.firstChild;
    while (child) {
      const hit = AutomationUtil.hitTest(child, point);
      if (hit) {
        return hit;
      }
      child = child.nextSibling;
    }

    const loc = node.unclippedLocation;

    // When |node| is partially or fully offscreen, try to find a better match.
    if (loc.left < 0 || loc.top < 0) {
      return null;
    }

    if (point.x <= (loc.left + loc.width) && point.x >= loc.left &&
        point.y <= (loc.top + loc.height) && point.y >= loc.top) {
      return node;
    }
    return null;
  }

  /**
   * Gets a top level root.
   * @param {!AutomationNode} node
   * @return {AutomationNode}
   */
  static getTopLevelRoot(node) {
    let root = node.root;
    if (!root || root.role == RoleType.DESKTOP) {
      return null;
    }

    while (root && root.parent && root.parent.root &&
           root.parent.root.role != RoleType.DESKTOP) {
      root = root.parent.root;
    }
    return root;
  }

  /**
   * @param {!AutomationNode} prevNode
   * @param {!AutomationNode} node
   * @return {AutomationNode}
   */
  static getLeastCommonAncestor(prevNode, node) {
    if (prevNode == node) {
      return node;
    }

    const prevAncestors = AutomationUtil.getAncestors(prevNode);
    const ancestors = AutomationUtil.getAncestors(node);
    const divergence = AutomationUtil.getDivergence(prevAncestors, ancestors);
    return ancestors[divergence - 1];
  }

  /**
   * Gets the accessible text for this node based on its role.
   * This text is suitable for caret navigation and selection in the node.
   * @param {AutomationNode} node
   * @return {string}
   */
  static getText(node) {
    if (!node) {
      return '';
    }

    if (node.role === RoleType.TEXT_FIELD) {
      return node.value || '';
    }
    return node.name || '';
  }

  /**
   * Gets the root of editable node.
   * @param {!AutomationNode} node
   * @return {!AutomationNode|undefined}
   */
  static getEditableRoot(node) {
    let testNode = node;
    let rootEditable;
    do {
      if (testNode.state.editable && testNode.state.focused) {
        rootEditable = testNode;
      }
      testNode = testNode.parent;
    } while (testNode);
    return rootEditable;
  }

  /**
   * Gets the last (DFS) ordered node matched by a predicate assuming a
   * preference for ancestors.
   *
   * In detail:
   * Given a DFS ordering on nodes a_1, ..., a_n, applying a predicate
   * from 1 to n yields a different set of nodes from that when applying
   * a predicate from n to 1 if we skip the remaining descendants of a
   * successfully matched node when moving forward. To recover the same
   * nodes when applying the predicate from n to 1, we make the
   * observation that we want the shallowest node that matches the
   * predicate in a successfully matched node's ancestry chain.
   * Note that container nodes should only be considered if there are no current
   * matches.
   * @param {!AutomationNode} root Tree to search.
   * @param {AutomationPredicate.Unary} pred A predicate to apply
   * @return {AutomationNode}
   */
  static findLastNode(root, pred) {
    let node = root;
    while (node.lastChild) {
      node = node.lastChild;
    }

    do {
      if (AutomationPredicate.shouldIgnoreNode(node)) {
        continue;
      }

      // Get the shallowest node matching the predicate.
      let walker = node;
      let shallowest = null;
      while (walker) {
        if (walker == root) {
          break;
        }

        if (pred(walker) && !AutomationPredicate.shouldIgnoreNode(walker) &&
            (!shallowest || !AutomationPredicate.container(walker))) {
          shallowest = walker;
        }

        walker = walker.parent;
      }

      if (shallowest) {
        return shallowest;
      }
    } while (node = AutomationUtil.findNextNode(node, Dir.BACKWARD, pred));

    return null;
  }
};
});  // goog.scope
