// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox predicates for the automation extension API.
 */

goog.provide('AutomationPredicate');
goog.provide('AutomationPredicate.Binary');
goog.provide('AutomationPredicate.Unary');

goog.require('constants');

goog.scope(function() {
const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const Restriction = chrome.automation.Restriction;
const Role = chrome.automation.RoleType;
const State = chrome.automation.StateType;

/**
 * A helper to find any actionable children.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
const hasActionableDescendant = function(node) {
  // DefaultActionVerb does not have value 'none' even though it gets set.
  if (node.defaultActionVerb != 'none') {
    return true;
  }

  let result = false;
  for (let i = 0; i < node.children.length; i++) {
    result = hasActionableDescendant(node.children[i]);
  }

  return result;
};

AutomationPredicate = class {
  constructor() {}

  /**
   * Constructs a predicate given a list of roles.
   * @param {!Array<Role>} roles
   * @return {!AutomationPredicate.Unary}
   */
  static roles(roles) {
    return AutomationPredicate.match({anyRole: roles});
  }

  /**
   * Constructs a predicate given a list of roles or predicates.
   * @param {{anyRole: (Array<Role>|undefined),
   *          anyPredicate: (Array<AutomationPredicate.Unary>|undefined),
   *          anyAttribute: (Object|undefined)}} params
   * @return {!AutomationPredicate.Unary}
   */
  static match(params) {
    const anyRole = params.anyRole || [];
    const anyPredicate = params.anyPredicate || [];
    const anyAttribute = params.anyAttribute || {};
    return function(node) {
      return anyRole.some(function(role) {
        return role == node.role;
      }) ||
          anyPredicate.some(function(p) {
            return p(node);
          }) ||
          Object.keys(anyAttribute).some(function(key) {
            return node[key] === anyAttribute[key];
          });
    };
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static button(node) {
    return /button/i.test(node.role);
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static editText(node) {
    return node.role == Role.TEXT_FIELD ||
        (node.state.editable && node.parent && !node.parent.state.editable);
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static image(node) {
    return node.role == Role.IMAGE && !!(node.name || node.url);
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static visitedLink(node) {
    return node.state[State.VISITED];
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static focused(node) {
    return node.state.focused;
  }

  /**
   * Returns true if this node should be considered a leaf for touch
   * exploration.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static touchLeaf(node) {
    return !!(!node.firstChild && node.name) || node.role == Role.BUTTON ||
        node.role == Role.POP_UP_BUTTON || node.role == Role.PORTAL ||
        node.role == Role.SLIDER || node.role == Role.TEXT_FIELD ||
        (node.role == Role.MENU_ITEM && !hasActionableDescendant(node));
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static leaf(node) {
    return AutomationPredicate.touchLeaf(node) ||
        // A node acting as a label should be a leaf if it has no actionable
        // controls.
        (!!node.labelFor && node.labelFor.length > 0 &&
         !hasActionableDescendant(node)) ||
        (!!node.descriptionFor && node.descriptionFor.length > 0 &&
         !hasActionableDescendant(node)) ||
        (node.activeDescendantFor && node.activeDescendantFor.length > 0) ||
        node.state[State.INVISIBLE] || node.children.every(function(n) {
          return n.state[State.INVISIBLE];
        }) ||
        // Explicitly only check the clickable attribute here (for Android).
        node.clickable || !!AutomationPredicate.math(node);
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static leafWithText(node) {
    return AutomationPredicate.leaf(node) && !!(node.name || node.value);
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static leafWithWordStop(node) {
    function hasWordStop(node) {
      if (node.role == Role.INLINE_TEXT_BOX) {
        return node.wordStarts && node.wordStarts.length;
      }

      // Non-text objects  are treated as having a single word stop.
      return true;
    }
    // Do not include static text leaves, which occur for an en end-of-line.
    return AutomationPredicate.leaf(node) && !node.state[State.INVISIBLE] &&
        node.role != Role.STATIC_TEXT && hasWordStop(node);
  }

  /**
   * Matches against leaves or static text nodes. Useful when restricting
   * traversal to non-inline textboxes while still allowing them if navigation
   * already entered into an inline textbox.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static leafOrStaticText(node) {
    return AutomationPredicate.leaf(node) || node.role == Role.STATIC_TEXT;
  }

  /**
   * Matches against nodes visited during object navigation. An object as
   * defined below, are all nodes that are focusable or static text. When used
   * in tree walking, it should visit all nodes that tab traversal would as well
   * as non-focusable static text.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static object(node) {
    // Editable nodes are within a text-like field and don't make sense when
    // performing object navigation. Users should use line, word, or character
    // navigation. Only navigate to the top level node.
    if (node.parent && node.parent.state.editable &&
        !node.parent.state[State.RICHLY_EDITABLE]) {
      return false;
    }

    // Given no other information, ChromeVox wants to visit focusable
    // (e.g. tabindex=0) nodes only when it has a name or is a control.
    if (node.state.focusable &&
        (node.name || node.state[State.EDITABLE] ||
         AutomationPredicate.formField(node))) {
      return true;
    }

    // Containers who have name from contents should be treated like objects if
    // the contents is all static text and not large.
    if (node.name && node.nameFrom == 'contents') {
      let onlyStaticText = true;
      let textLength = 0;
      for (let i = 0, child; child = node.children[i]; i++) {
        if (child.role != Role.STATIC_TEXT) {
          onlyStaticText = false;
          break;
        }
        textLength += child.name ? child.name.length + textLength : textLength;
      }

      if (onlyStaticText && textLength > 0 &&
          textLength < constants.OBJECT_MAX_CHARCOUNT) {
        return true;
      }
    }

    // Otherwise, leaf or static text nodes that don't contain only whitespace
    // should be visited with the exception of non-text only nodes. This covers
    // cases where an author might make a link with a name of ' '.
    return AutomationPredicate.leafOrStaticText(node) &&
        (/\S+/.test(node.name) ||
         (node.role != Role.LINE_BREAK && node.role != Role.STATIC_TEXT &&
          node.role != Role.INLINE_TEXT_BOX));
  }

  /**
   * Matches against nodes visited during touch exploration.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static touchObject(node) {
    // Exclude large objects such as containers.
    if (AutomationPredicate.container(node)) {
      return false;
    }

    return AutomationPredicate.object(node);
  }

  /**
   * @param {!AutomationNode} first
   * @param {!AutomationNode} second
   * @return {boolean}
   */
  static linebreak(first, second) {
    // TODO(dtseng): Use next/previousOnLine once available.
    const fl = first.unclippedLocation;
    const sl = second.unclippedLocation;
    return fl.top != sl.top || (fl.top + fl.height != sl.top + sl.height);
  }

  /**
   * Matches against a node that contains other interesting nodes.
   * These nodes should always have their subtrees scanned when navigating.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static container(node) {
    // Math is never a container.
    if (AutomationPredicate.math(node)) {
      return false;
    }

    // Clickables (on Android) are not containers.
    if (node.clickable) {
      return false;
    }

    return AutomationPredicate.match({
      anyRole: [
        Role.GENERIC_CONTAINER, Role.DOCUMENT, Role.GROUP, Role.LIST,
        Role.LIST_ITEM, Role.TOOLBAR, Role.WINDOW
      ],
      anyPredicate: [
        AutomationPredicate.landmark, AutomationPredicate.structuralContainer,
        function(node) {
          // For example, crosh.
          return node.role == Role.TEXT_FIELD &&
              node.restriction == Restriction.READ_ONLY;
        },
        function(node) {
          return (
              node.state.editable && node.parent &&
              !node.parent.state.editable);
        }
      ]
    })(node);
  }

  /**
   * Returns whether the given node should not be crossed when performing
   * traversals up the ancestry chain.
   * @param {AutomationNode} node
   * @return {boolean}
   */
  static root(node) {
    switch (node.role) {
      case Role.WINDOW:
        return true;
      case Role.DIALOG:
        if (node.root.role != Role.DESKTOP) {
          return !!node.modal;
        }

        // The below logic handles nested dialogs properly in the desktop tree
        // like that found in a bubble view.
        return !!node.parent && node.parent.role == Role.WINDOW &&
            node.parent.children.every(function(child) {
              return node.role == Role.WINDOW || node.role == Role.DIALOG;
            });
      case Role.TOOLBAR:
        return node.root.role == Role.DESKTOP &&
            !(node.nextFocus || !node.previousFocus);
      case Role.ROOT_WEB_AREA:
        if (node.parent && node.parent.role == Role.WEB_VIEW &&
            !node.parent.state[State.FOCUSED]) {
          // If parent web view is not focused, we should allow this root web
          // area to be crossed when performing traversals up the ancestry
          // chain.
          return false;
        }
        return !node.parent || !node.parent.root ||
            (node.parent.root.role == Role.DESKTOP &&
             node.parent.role == Role.WEB_VIEW);
      default:
        return !!node.modal;
    }
  }

  /**
   * Returns whether the given node should not be crossed when performing
   * traversal inside of an editable. Note that this predicate should not be
   * applied everywhere since there would be no way for a user to exit the
   * editable.
   * @param {AutomationNode} node
   * @return {boolean}
   */
  static rootOrEditableRoot(node) {
    return AutomationPredicate.root(node) ||
        (node.state.richlyEditable && node.state.focused &&
         node.children.length > 0);
  }

  /**
   * Nodes that should be ignored while traversing the automation tree. For
   * example, apply this predicate when moving to the next object.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static shouldIgnoreNode(node) {
    // Ignore invisible nodes.
    if (node.state.invisible ||
        (node.location.height == 0 && node.location.width == 0)) {
      return true;
    }

    // Ignore structural containres.
    if (AutomationPredicate.structuralContainer(node)) {
      return true;
    }

    // Ignore nodes acting as labels for another control, that don't
    // have actionable descendants.
    if (node.labelFor && node.labelFor.length > 0 &&
        !hasActionableDescendant(node)) {
      return true;
    }

    // Similarly, ignore nodes acting as descriptions.
    if (node.descriptionFor && node.descriptionFor.length > 0 &&
        !hasActionableDescendant(node)) {
      return true;
    }

    // Don't ignore nodes with names or name-like attribute.
    if (node.name || node.value || node.description || node.url) {
      return false;
    }

    // Don't ignore math nodes.
    if (AutomationPredicate.math(node)) {
      return false;
    }

    // Ignore some roles.
    return AutomationPredicate.leaf(node) && (AutomationPredicate.roles([
             Role.CLIENT, Role.COLUMN, Role.GENERIC_CONTAINER, Role.GROUP,
             Role.IMAGE, Role.PARAGRAPH, Role.STATIC_TEXT, Role.SVG_ROOT,
             Role.TABLE_HEADER_CONTAINER, Role.UNKNOWN
           ])(node));
  }

  /**
   * Returns if the node has a meaningful checked state.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static checkable(node) {
    return !!node.checked;
  }

  /**
   * Returns a predicate that will match against the directed next cell taking
   * into account the current ancestor cell's position in the table.
   * @param {AutomationNode} start
   * @param {{dir: (Dir|undefined),
   *           row: (boolean|undefined),
   *          col: (boolean|undefined)}} opts
   * |dir|, specifies direction for |row or/and |col| movement by one cell.
   *     |dir| defaults to forward.
   *     |row| and |col| are both false by default.
   *     |end| defaults to false. If set to true, |col| must also be set to
   * true. It will then return the first or last cell in the current column.
   * @return {?AutomationPredicate.Unary} Returns null if not in a table.
   */
  static makeTableCellPredicate(start, opts) {
    if (!opts.row && !opts.col) {
      throw new Error('You must set either row or col to true');
    }

    const dir = opts.dir || Dir.FORWARD;

    // Compute the row/col index defaulting to 0.
    let rowIndex = 0, colIndex = 0;
    let tableNode = start;
    while (tableNode) {
      if (AutomationPredicate.table(tableNode)) {
        break;
      }

      if (AutomationPredicate.cellLike(tableNode)) {
        rowIndex = tableNode.tableCellRowIndex;
        colIndex = tableNode.tableCellColumnIndex;
      }

      tableNode = tableNode.parent;
    }
    if (!tableNode) {
      return null;
    }

    // Only support making a predicate for column ends.
    if (opts.end) {
      if (!opts.col) {
        throw 'Unsupported option.';
      }

      if (dir == Dir.FORWARD) {
        return function(node) {
          return AutomationPredicate.cellLike(node) &&
              node.tableCellColumnIndex == colIndex &&
              node.tableCellRowIndex >= 0;
        };
      } else {
        return function(node) {
          return AutomationPredicate.cellLike(node) &&
              node.tableCellColumnIndex == colIndex &&
              node.tableCellRowIndex < tableNode.tableRowCount;
        };
      }
    }

    // Adjust for the next/previous row/col.
    if (opts.row) {
      rowIndex = dir == Dir.FORWARD ? rowIndex + 1 : rowIndex - 1;
    }
    if (opts.col) {
      colIndex = dir == Dir.FORWARD ? colIndex + 1 : colIndex - 1;
    }

    return function(node) {
      return AutomationPredicate.cellLike(node) &&
          node.tableCellColumnIndex == colIndex &&
          node.tableCellRowIndex == rowIndex;
    };
  }

  /**
   * Returns a predicate that will match against a heading with a specific
   * hierarchical level.
   * @param {number} level 1-6
   * @return {AutomationPredicate.Unary}
   */
  static makeHeadingPredicate(level) {
    return function(node) {
      return node.role == Role.HEADING && node.hierarchicalLevel == level;
    };
  }

  /**
   * Matches against a node that forces showing surrounding contextual
   * information for braille.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static contextualBraille(node) {
    return node.parent != null &&
        ((node.parent.role == Role.ROW && AutomationPredicate.cellLike(node)) ||
         (node.parent.role == Role.TREE &&
          node.parent.state[State.HORIZONTAL]));
  }

  /**
   * Matches against a node that handles multi line key commands.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static multiline(node) {
    return node.state[State.MULTILINE] || node.state[State.RICHLY_EDITABLE];
  }

  /**
   * Matches against a node that should be auto-scrolled during navigation.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static autoScrollable(node) {
    return !!node.scrollable &&
        (node.standardActions.includes(
             chrome.automation.ActionType.SCROLL_FORWARD) ||
         node.standardActions.includes(
             chrome.automation.ActionType.SCROLL_BACKWARD)) &&
        (node.role == Role.GRID || node.role == Role.LIST ||
         node.role == Role.POP_UP_BUTTON || node.role == Role.SCROLL_VIEW);
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static math(node) {
    return node.role == Role.MATH || !!node.htmlAttributes['data-mathml'];
  }

  /**
   * Matches against editable nodes, that should not be treated in the usual
   * fashion.
   * Instead, only output the contents around the selection in braille.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static shouldOnlyOutputSelectionChangeInBraille(node) {
    return node.state[State.RICHLY_EDITABLE] && node.state[State.FOCUSED] &&
        node.role == Role.LOG;
  }

  /**
   * Matches against nodes we should ignore in a jump command.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static ignoreDuringJump(node) {
    return node.role == Role.GENERIC_CONTAINER ||
        node.role == Role.STATIC_TEXT || node.role == Role.INLINE_TEXT_BOX;
  }

  /**
   * Returns a predicate that will match against a list-like node. The returned
   * predicate should not match the first list-like ancestor of |node| (or
   * |node| itself, if it is list-like).
   * @param {AutomationNode} node
   * @return {AutomationPredicate.Unary}
   */
  static makeListPredicate(node) {
    // Scan upward for a list-like ancestor. We do not want to match against
    // this node.
    let avoidNode = node;
    while (avoidNode && !AutomationPredicate.listLike(avoidNode)) {
      avoidNode = avoidNode.parent;
    }

    return function(autoNode) {
      return AutomationPredicate.listLike(autoNode) && (autoNode !== avoidNode);
    };
  }
};

/**
 * @typedef {function(!AutomationNode) : boolean}
 */
AutomationPredicate.Unary;

/**
 * @typedef {function(!AutomationNode,
 * !AutomationNode) : boolean}
 */
AutomationPredicate.Binary;


/** @type {AutomationPredicate.Unary} */
AutomationPredicate.checkBox =
    AutomationPredicate.roles([Role.CHECK_BOX, Role.SWITCH]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.comboBox = AutomationPredicate.roles([
  Role.COMBO_BOX_GROUPING, Role.COMBO_BOX_MENU_BUTTON,
  Role.TEXT_FIELD_WITH_COMBO_BOX, Role.POP_UP_BUTTON, Role.MENU_LIST_POPUP
]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.heading = AutomationPredicate.roles([Role.HEADING]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.inlineTextBox =
    AutomationPredicate.roles([Role.INLINE_TEXT_BOX]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.link = AutomationPredicate.roles([Role.LINK]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.row = AutomationPredicate.roles([Role.ROW]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.table =
    AutomationPredicate.roles([Role.GRID, Role.LIST_GRID, Role.TABLE]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.listLike =
    AutomationPredicate.roles([Role.LIST, Role.DESCRIPTION_LIST]);


/** @type {AutomationPredicate.Unary} */
AutomationPredicate.formField = AutomationPredicate.match({
  anyPredicate: [
    AutomationPredicate.button, AutomationPredicate.comboBox,
    AutomationPredicate.editText
  ],
  anyRole: [
    Role.CHECK_BOX, Role.COLOR_WELL, Role.LIST_BOX, Role.SLIDER, Role.SWITCH,
    Role.TAB, Role.TREE
  ]
});

/** @type {AutomationPredicate.Unary} */
AutomationPredicate.control = AutomationPredicate.match({
  anyPredicate: [
    AutomationPredicate.formField,
  ],
  anyRole: [
    Role.DISCLOSURE_TRIANGLE, Role.MENU_ITEM, Role.MENU_ITEM_CHECK_BOX,
    Role.MENU_ITEM_RADIO, Role.MENU_LIST_OPTION, Role.SCROLL_BAR
  ]
});


/** @type {AutomationPredicate.Unary} */
AutomationPredicate.linkOrControl = AutomationPredicate.match(
    {anyPredicate: [AutomationPredicate.control], anyRole: [Role.LINK]});

/** @type {AutomationPredicate.Unary} */
AutomationPredicate.landmark = AutomationPredicate.roles([
  Role.APPLICATION, Role.BANNER, Role.COMPLEMENTARY, Role.CONTENT_INFO,
  Role.FORM, Role.MAIN, Role.NAVIGATION, Role.REGION, Role.SEARCH
]);


/**
 * Matches against nodes visited during group navigation. An object as
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.group = AutomationPredicate.match({
  anyRole: [Role.HEADING, Role.LIST, Role.PARAGRAPH],
  anyPredicate: [
    AutomationPredicate.editText, AutomationPredicate.formField,
    AutomationPredicate.object, AutomationPredicate.table
  ]
});


/**
 * Matches against nodes that contain interesting nodes, but should never be
 * visited.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.structuralContainer = AutomationPredicate.roles([
  Role.ALERT_DIALOG, Role.CLIENT, Role.DIALOG, Role.LAYOUT_TABLE,
  Role.LAYOUT_TABLE_CELL, Role.LAYOUT_TABLE_ROW, Role.ROOT_WEB_AREA,
  Role.WEB_VIEW, Role.WINDOW, Role.EMBEDDED_OBJECT, Role.IFRAME,
  Role.IFRAME_PRESENTATIONAL, Role.PLUGIN_OBJECT, Role.IGNORED, Role.UNKNOWN
]);


/**
 * Returns if the node is clickable.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.clickable = AutomationPredicate.match({
  anyPredicate: [
    AutomationPredicate.button, AutomationPredicate.link,
    (node) => {
      return node.defaultActionVerb ==
          chrome.automation.DefaultActionVerb.CLICK;
    }
  ],
  anyAttribute: {clickable: true}
});

// Table related predicates.
/**
 * Returns if the node has a cell like role.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.cellLike =
    AutomationPredicate.roles([Role.CELL, Role.ROW_HEADER, Role.COLUMN_HEADER]);


/**
 * Matches against nodes that we may be able to retrieve image data from.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.supportsImageData =
    AutomationPredicate.roles([Role.CANVAS, Role.IMAGE, Role.VIDEO]);


/**
 * Matches against menu item like nodes.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.menuItem = AutomationPredicate.roles(
    [Role.MENU_ITEM, Role.MENU_ITEM_CHECK_BOX, Role.MENU_ITEM_RADIO]);

/**
 * Matches against text like nodes.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.text = AutomationPredicate.roles(
    [Role.STATIC_TEXT, Role.INLINE_TEXT_BOX, Role.LINE_BREAK]);
});  // goog.scope
