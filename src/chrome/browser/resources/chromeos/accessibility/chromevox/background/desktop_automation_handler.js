// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation from a desktop automation node.
 */

goog.provide('DesktopAutomationHandler');

goog.require('AutomationObjectConstructorInstaller');
goog.require('BaseAutomationHandler');
goog.require('ChromeVoxState');
goog.require('CommandHandler');
goog.require('CustomAutomationEvent');
goog.require('editing.TextEditHandler');

goog.scope(function() {
const ActionType = chrome.automation.ActionType;
const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

DesktopAutomationHandler = class extends BaseAutomationHandler {
  /**
   * @param {!AutomationNode} node
   */
  constructor(node) {
    super(node);

    /**
     * The object that speaks changes to an editable text field.
     * @type {editing.TextEditHandler}
     * @private
     */
    this.textEditHandler_ = null;

    /**
     * The last time we handled a value changed event.
     * @type {!Date}
     * @private
     */
    this.lastValueChanged_ = new Date(0);

    /** @private {AutomationNode} */
    this.lastValueTarget_ = null;

    /**
     * The last time we handled an alert event.
     * @type {!Date}
     * @private
     */
    this.lastAlertTime_ = new Date(0);

    /** @private {string} */
    this.lastAlertText_ = '';

    /**
     * The last time we handled a live region changed event.
     * @type {!Date}
     * @private
     */
    this.liveRegionChange_ = new Date();

    /** @private {string}*/
    this.lastLiveRegionChangeText_ = '';

    /** @private {string} */
    this.lastRootUrl_ = '';

    /** @private {boolean} */
    this.shouldIgnoreDocumentSelectionFromAction_ = false;

    /** @private {number?} */
    this.delayedAttributeOutputId_;

    /** @private {number} */
    this.currentPage_ = -1;
    /** @private {number} */
    this.totalPages_ = -1;

    this.addListener_(EventType.ALERT, this.onAlert);
    this.addListener_(EventType.BLUR, this.onBlur);
    this.addListener_(
        EventType.DOCUMENT_SELECTION_CHANGED, this.onDocumentSelectionChanged);
    this.addListener_(EventType.FOCUS, this.onFocus);

    // Note that live region changes from views are really announcement
    // events. Their target nodes contain no live region semantics and have no
    // relation to live regions which are supported in |LiveRegions|.
    this.addListener_(EventType.LIVE_REGION_CHANGED, this.onLiveRegionChanged);

    this.addListener_(EventType.LOAD_COMPLETE, this.onLoadComplete);
    this.addListener_(EventType.FOCUS_AFTER_MENU_CLOSE, this.onMenuEnd);
    this.addListener_(EventType.MENU_START, (event) => {
      Output.forceModeForNextSpeechUtterance(QueueMode.CATEGORY_FLUSH);
      this.onEventDefault(event);
    });
    this.addListener_(EventType.RANGE_VALUE_CHANGED, this.onValueChanged);
    this.addListener_(
        EventType.SCROLL_POSITION_CHANGED, this.onScrollPositionChanged);
    this.addListener_(
        EventType.SCROLL_HORIZONTAL_POSITION_CHANGED,
        this.onScrollPositionChanged);
    this.addListener_(
        EventType.SCROLL_VERTICAL_POSITION_CHANGED,
        this.onScrollPositionChanged);
    // Called when a same-page link is followed or the url fragment changes.
    this.addListener_(EventType.SCROLLED_TO_ANCHOR, this.onScrolledToAnchor);
    this.addListener_(EventType.SELECTION, this.onSelection);
    this.addListener_(
        EventType.TEXT_SELECTION_CHANGED, this.onEditableChanged_);
    this.addListener_(
        EventType.VALUE_IN_TEXT_FIELD_CHANGED, this.onEditableChanged_);
    this.addListener_(EventType.VALUE_CHANGED, this.onValueChanged);

    AutomationObjectConstructorInstaller.init(node, function() {
      chrome.automation.getFocus((function(focus) {
                                   if (focus) {
                                     const event = new CustomAutomationEvent(
                                         EventType.FOCUS, focus, {
                                           eventFrom: 'page',
                                           eventFromAction: ActionType.FOCUS
                                         });
                                     this.onFocus(event);
                                   }
                                 }).bind(this));
    }.bind(this));
  }

  /** @type {editing.TextEditHandler} */
  get textEditHandler() {
    return this.textEditHandler_;
  }

  /** @override */
  willHandleEvent_(evt) {
    return false;
  }

  /**
   * Handles the result of a hit test.
   * @param {!AutomationNode} node The hit result.
   */
  onHitTestResult(node) {
    // It's possible the |node| hit has lost focus (via its root).
    const host = node.root.parent;
    if (node.parent && host && host.role === RoleType.WEB_VIEW &&
        !host.state.focused) {
      return;
    }

    // It is possible that the user moved since we requested a hit test.  Bail
    // if the current range is valid and on the same page as the hit result
    // (but not the root).
    if (ChromeVoxState.instance.currentRange &&
        ChromeVoxState.instance.currentRange.start &&
        ChromeVoxState.instance.currentRange.start.node &&
        ChromeVoxState.instance.currentRange.start.node.root) {
      const cur = ChromeVoxState.instance.currentRange.start.node;
      if (cur.role !== RoleType.ROOT_WEB_AREA &&
          AutomationUtil.getTopLevelRoot(node) ===
              AutomationUtil.getTopLevelRoot(cur)) {
        return;
      }
    }

    chrome.automation.getFocus(function(focus) {
      if (!focus && !node) {
        return;
      }

      focus = node || focus;
      const focusedRoot = AutomationUtil.getTopLevelRoot(focus);
      const output = new Output();
      if (focus !== focusedRoot && focusedRoot) {
        output.format('$name', focusedRoot);
      }

      // Even though we usually don't output events from actions, hit test
      // results should generate output.
      const range = cursors.Range.fromNode(focus);
      ChromeVoxState.instance.setCurrentRange(range);
      output.withRichSpeechAndBraille(range, null, OutputEventType.NAVIGATE)
          .go();
    });
  }

  /**
   * Makes an announcement without changing focus.
   * @param {!ChromeVoxEvent} evt
   */
  onAlert(evt) {
    const node = evt.target;

    if (node.role === RoleType.ALERT && node.root.role === RoleType.DESKTOP) {
      // Exclude alerts in the desktop tree that are inside of menus.
      let ancestor = node;
      while (ancestor) {
        if (ancestor.role === RoleType.MENU) {
          return;
        }
        ancestor = ancestor.parent;
      }
    }

    const range = cursors.Range.fromNode(node);
    const output = new Output()
                       .withSpeechCategory(TtsCategory.LIVE)
                       .withSpeechAndBraille(range, null, evt.type);

    const alertDelayMet = new Date() - this.lastAlertTime_ >
        DesktopAutomationHandler.MIN_ALERT_DELAY_MS;
    if (!alertDelayMet && output.toString() === this.lastAlertText_) {
      return;
    }

    this.lastAlertTime_ = new Date();
    this.lastAlertText_ = output.toString();

    // A workaround for alert nodes that contain no actual content.
    if (output.toString()) {
      output.go();
    }
  }

  onBlur(evt) {
    // Nullify focus if it no longer exists.
    chrome.automation.getFocus(function(focus) {
      if (!focus) {
        ChromeVoxState.instance.setCurrentRange(null);
      }
    });
  }

  /**
   * @param {!ChromeVoxEvent} evt
   */
  onDocumentSelectionChanged(evt) {
    let selectionStart = evt.target.selectionStartObject;

    // No selection.
    if (!selectionStart) {
      return;
    }

    // A caller requested this event be ignored.
    if (this.shouldIgnoreDocumentSelectionFromAction_ &&
        evt.eventFrom === 'action') {
      return;
    }

    // Editable selection.
    if (selectionStart.state[StateType.EDITABLE]) {
      selectionStart =
          AutomationUtil.getEditableRoot(selectionStart) || selectionStart;
      this.onEditableChanged_(
          new CustomAutomationEvent(evt.type, selectionStart, {
            eventFrom: evt.eventFrom,
            eventFromAction: evt.eventFromAction,
            intents: evt.intents
          }));
    }

    // Non-editable selections are handled in |Background|.
  }

  /**
   * Provides all feedback once a focus event fires.
   * @param {!ChromeVoxEvent} evt
   */
  onFocus(evt) {
    let node = evt.target;
    const isRootWebArea = node.role === RoleType.ROOT_WEB_AREA;
    const isFrame = isRootWebArea && node.parent && node.parent.root &&
        node.parent.root.role === RoleType.ROOT_WEB_AREA;
    if (isRootWebArea && !isFrame && evt.eventFrom !== 'action') {
      chrome.automation.getFocus(
          this.maybeRecoverFocusAndOutput_.bind(this, evt));
      return;
    }

    // Invalidate any previous editable text handler state.
    if (!this.createTextEditHandlerIfNeeded_(evt.target, true)) {
      this.textEditHandler_ = null;
    }

    // Discard focus events on embeddedObject and webView.
    if (node.role === RoleType.EMBEDDED_OBJECT ||
        node.role === RoleType.PLUGIN_OBJECT ||
        node.role === RoleType.WEB_VIEW) {
      return;
    }

    if (node.role === RoleType.UNKNOWN) {
      // Ideally, we'd get something more meaningful than focus on an unknown
      // node, but this does sometimes occur. Sync downward to a more reasonable
      // target.
      node = AutomationUtil.findNodePre(
          node, Dir.FORWARD, AutomationPredicate.object);

      if (!node) {
        return;
      }
    }

    if (!node.root) {
      return;
    }

    // Update the focused root url, which gets used as part of focus recovery.
    this.lastRootUrl_ = node.root.docUrl || '';

    // Consider the case when a user presses tab rapidly. The key events may
    // come in far before the accessibility focus events. We therefore must
    // category flush here or the focus events will all queue up.
    Output.forceModeForNextSpeechUtterance(QueueMode.CATEGORY_FLUSH);

    const event = new CustomAutomationEvent(EventType.FOCUS, node, {
      eventFrom: evt.eventFrom,
      eventFromAction: evt.eventFromAction,
      intents: evt.intents
    });
    this.onEventDefault(event);

    // Refresh the handler, if needed, now that ChromeVox focus is up to date.
    this.createTextEditHandlerIfNeeded_(node);
  }

  /**
   * @param {!ChromeVoxEvent} evt
   */
  onLiveRegionChanged(evt) {
    if (evt.target.root.role === RoleType.DESKTOP ||
        evt.target.root.role === RoleType.APPLICATION) {
      if (evt.target.containerLiveStatus !== 'assertive' &&
          evt.target.containerLiveStatus !== 'polite') {
        return;
      }

      const output = new Output();
      if (evt.target.containerLiveStatus === 'assertive') {
        output.withQueueMode(QueueMode.CATEGORY_FLUSH);
      } else {
        output.withQueueMode(QueueMode.QUEUE);
      }
      const liveRegionChange = (new Date() - this.liveRegionChange_) <
          DesktopAutomationHandler.LIVE_REGION_DELAY_MS;

      output
          .withRichSpeechAndBraille(
              cursors.Range.fromNode(evt.target), null, evt.type)
          .withSpeechCategory(TtsCategory.LIVE);
      if (liveRegionChange &&
          output.toString() === this.lastLiveRegionChangeText_) {
        return;
      }

      this.liveRegionChange_ = new Date();
      this.lastLiveRegionChangeText_ = output.toString();
      output.go();
    }
  }

  /**
   * Provides all feedback once a load complete event fires.
   * @param {!ChromeVoxEvent} evt
   */
  onLoadComplete(evt) {
    // A load complete gets fired on the desktop node when display metrics
    // change.
    if (evt.target.role === RoleType.DESKTOP) {
      const msg = evt.target.state[StateType.HORIZONTAL] ? 'device_landscape' :
                                                           'device_portrait';
      new Output().format('@' + msg).go();
      return;
    }

    // We are only interested in load completes on valid top level roots.
    const top = AutomationUtil.getTopLevelRoot(evt.target);
    if (!top || top !== evt.target.root || !top.docUrl) {
      return;
    }

    this.lastRootUrl_ = '';
    chrome.automation.getFocus(function(focus) {
      // In some situations, ancestor windows get focused before a descendant
      // webView/rootWebArea. In particular, a window that gets opened but no
      // inner focus gets set. We catch this generically by re-targetting focus
      // if focus is the ancestor of the load complete target (below).
      const focusIsAncestor = AutomationUtil.isDescendantOf(evt.target, focus);
      const focusIsDescendant =
          AutomationUtil.isDescendantOf(focus, evt.target);
      if (!focus || (!focusIsAncestor && !focusIsDescendant)) {
        return;
      }

      if (focusIsAncestor) {
        focus = evt.target;
      }

      // Create text edit handler, if needed, now in order not to miss initial
      // value change if text field has already been focused when initializing
      // ChromeVox.
      this.createTextEditHandlerIfNeeded_(focus);

      // If auto read is set, skip focus recovery and start reading from the
      // top.
      if (localStorage['autoRead'] === 'true' &&
          AutomationUtil.getTopLevelRoot(evt.target) === evt.target) {
        ChromeVoxState.instance.setCurrentRange(
            cursors.Range.fromNode(evt.target));
        ChromeVox.tts.stop();
        CommandHandler.onCommand('readFromHere');
        return;
      }

      this.maybeRecoverFocusAndOutput_(evt, focus);
    }.bind(this));
  }

  /**
   * Sets whether document selections from actions should be ignored.
   * @param {boolean} val
   */
  ignoreDocumentSelectionFromAction(val) {
    this.shouldIgnoreDocumentSelectionFromAction_ = val;
  }

  /**
   * Provides all feedback once a change event in a text field fires.
   * @param {!ChromeVoxEvent} evt
   * @private
   */
  onEditableChanged_(evt) {
    if (!evt.target.state.editable) {
      return;
    }

    // Skip all unfocused text fields.
    if (!evt.target.state[StateType.FOCUSED] &&
        evt.target.state[StateType.EDITABLE]) {
      return;
    }

    const isInput = evt.target.htmlTag === 'input';
    const isTextArea = evt.target.htmlTag === 'textarea';
    const isContentEditable = evt.target.state[StateType.RICHLY_EDITABLE];

    switch (evt.type) {
      case EventType.DOCUMENT_SELECTION_CHANGED:
        // Event type DOCUMENT_SELECTION_CHANGED is duplicated by
        // TEXT_SELECTION_CHANGED for <input> elements.
        if (isInput) {
          return;
        }
        break;
      case EventType.FOCUS:
        // Allowed regardless of the role.
        break;
      case EventType.TEXT_SELECTION_CHANGED:
        // Event type TEXT_SELECTION_CHANGED is duplicated by
        // DOCUMENT_SELECTION_CHANGED for content editables and text areas.
        // Fall through.
      case EventType.VALUE_IN_TEXT_FIELD_CHANGED:
        // By design, generated only for simple inputs.
        if (isContentEditable || isTextArea) {
          return;
        }
        break;
      case EventType.VALUE_CHANGED:
        // During a transition period, VALUE_CHANGED is duplicated by
        // VALUE_IN_TEXT_FIELD_CHANGED for text field roles.
        //
        // TOTO(NEKTAR): Deprecate and remove VALUE_CHANGED.
        if (isContentEditable || isInput || isTextArea) {
          return;
        }
      default:
        return;
    }

    if (!this.createTextEditHandlerIfNeeded_(evt.target)) {
      return;
    }

    if (!ChromeVoxState.instance.currentRange) {
      this.onEventDefault(evt);
      ChromeVoxState.instance.setCurrentRange(
          cursors.Range.fromNode(evt.target));
    }

    // Sync the ChromeVox range to the editable, if a selection exists.
    const selectionStartObject = evt.target.root.selectionStartObject;
    const selectionStartOffset = evt.target.root.selectionStartOffset || 0;
    const selectionEndObject = evt.target.root.selectionEndObject;
    const selectionEndOffset = evt.target.root.selectionEndOffset || 0;
    if (selectionStartObject && selectionEndObject) {
      // Sync to the selection's deep equivalent especially in editables, where
      // selection is often on the root text field with a child offset.
      const selectedRange = new cursors.Range(
          new cursors.WrappingCursor(selectionStartObject, selectionStartOffset)
              .deepEquivalent,
          new cursors.WrappingCursor(selectionEndObject, selectionEndOffset)
              .deepEquivalent);

      // Sync ChromeVox range with selection.
      ChromeVoxState.instance.setCurrentRange(selectedRange);
    }
    this.textEditHandler_.onEvent(evt);
  }

  /**
   * Provides all feedback once a rangeValueChanged or a valueInTextFieldChanged
   * event fires.
   * @param {!ChromeVoxEvent} evt
   */
  onValueChanged(evt) {
    // Skip root web areas.
    if (evt.target.role === RoleType.ROOT_WEB_AREA) {
      return;
    }

    // Delegate to the edit text handler if this is an editable, with the
    // exception of spin buttons.
    if (evt.target.state[StateType.EDITABLE] &&
        evt.target.role !== RoleType.SPIN_BUTTON) {
      this.onEditableChanged_(evt);
      return;
    }

    const t = evt.target;
    const fromDesktop = t.root.role === RoleType.DESKTOP;
    const onDesktop =
        ChromeVoxState.instance.currentRange.start.node.root.role ===
        RoleType.DESKTOP;
    if (fromDesktop && !onDesktop && t.role !== RoleType.SLIDER) {
      // Only respond to value changes from the desktop if it's coming from a
      // slider e.g. the volume slider. Do this to avoid responding to frequent
      // updates from UI e.g. download progress bars.
      return;
    }
    if (t.state.focused || fromDesktop ||
        AutomationUtil.isDescendantOf(
            ChromeVoxState.instance.currentRange.start.node, t)) {
      if (new Date() - this.lastValueChanged_ <=
          DesktopAutomationHandler.MIN_VALUE_CHANGE_DELAY_MS) {
        return;
      }

      this.lastValueChanged_ = new Date();

      const output = new Output();
      output.withoutFocusRing();

      if (fromDesktop &&
          (!this.lastValueTarget_ || this.lastValueTarget_ !== t)) {
        const range = cursors.Range.fromNode(t);
        output.withRichSpeechAndBraille(range, range, OutputEventType.NAVIGATE);
        this.lastValueTarget_ = t;
      } else {
        output.format(
            '$if($value, $value, $if($valueForRange, $valueForRange))', t);
      }

      Output.forceModeForNextSpeechUtterance(QueueMode.INTERJECT);
      output.go();
    }
  }

  /**
   * Handle updating the active indicator when the document scrolls.
   * @param {!ChromeVoxEvent} evt
   */
  onScrollPositionChanged(evt) {
    const currentRange = ChromeVoxState.instance.currentRange;
    if (currentRange && currentRange.isValid()) {
      new Output().withLocation(currentRange, null, evt.type).go();

      if (EventSourceState.get() !== EventSourceType.TOUCH_GESTURE) {
        return;
      }

      const root = AutomationUtil.getTopLevelRoot(currentRange.start.node);
      if (!root || root.scrollY === undefined) {
        return;
      }

      const currentPage = Math.ceil(root.scrollY / root.location.height) || 1;
      const totalPages =
          Math.ceil(
              (root.scrollYMax - root.scrollYMin) / root.location.height) ||
          1;

      // Ignore announcements if we've already announced something for this page
      // change. Note that this need not care about the root if it changed as
      // well.
      if (this.currentPage_ === currentPage &&
          this.totalPages_ === totalPages) {
        return;
      }
      this.currentPage_ = currentPage;
      this.totalPages_ = totalPages;
      ChromeVox.tts.speak(
          Msgs.getMsg('describe_pos_by_page', [currentPage, totalPages]),
          QueueMode.QUEUE);
    }
  }

  /**
   * @param {!ChromeVoxEvent} evt
   */
  onSelection(evt) {
    // Invalidate any previous editable text handler state since some nodes,
    // like menuitems, can receive selection while focus remains on an
    // editable leading to braille output routing to the editable.
    this.textEditHandler_ = null;

    chrome.automation.getFocus((focus) => {
      const target = evt.target;

      // Desktop tabs get "selection" when there's a focused webview during
      // tab switching. Read it, but don't steal focus which is on the
      // omnibox. We have to resort to this check to get tab switching read out
      // because on switching to a new tab, focus actually remains on the *same*
      // omnibox.
      const currentRange = ChromeVoxState.instance.currentRange;
      if (target.role === RoleType.TAB &&
          target.root.role === RoleType.DESKTOP && currentRange &&
          currentRange.start && currentRange.start.node &&
          currentRange.start.node.className === 'OmniboxViewViews') {
        const range = cursors.Range.fromNode(target);
        new Output()
            .withRichSpeechAndBraille(range, range, OutputEventType.NAVIGATE)
            .go();
        return;
      }

      let override = false;
      const isDesktop =
          (target.root === focus.root && focus.root.role === RoleType.DESKTOP);

      // TableView fires selection events on rows/cells
      // and we want to ignore those because it also fires focus events.
      if (isDesktop && target.role === RoleType.CELL ||
          target.role === RoleType.ROW) {
        return;
      }

      // Menu items and IME candidates always announce on selection events,
      // independent of focus.
      if (AutomationPredicate.menuItem(target) ||
          target.role === RoleType.IME_CANDIDATE) {
        override = true;
      }

      // Overview mode should allow selections.
      if (isDesktop) {
        let walker = target;
        while (walker && walker.className !== 'VirtualDesksWidget' &&
               walker.className !== 'OverviewModeLabel' &&
               walker.className !== 'Desk_Container_A') {
          walker = walker.parent;
        }

        override = !!walker || override;
      }

      // The popup view associated with a datalist element does not descend
      // from the input with which it is associated.
      if (focus.role === RoleType.TEXT_FIELD_WITH_COMBO_BOX &&
          target.role === RoleType.LIST_BOX_OPTION) {
        override = true;
      }

      if (override || AutomationUtil.isDescendantOf(target, focus)) {
        this.onEventDefault(evt);
      }
    });
  }

  /**
   * Provides all feedback once a menu end event fires.
   * @param {!ChromeVoxEvent} evt
   */
  onMenuEnd(evt) {
    // This is a work around for Chrome context menus not firing a focus event
    // after you close them.
    chrome.automation.getFocus(function(focus) {
      if (focus) {
        // Directly output the node here; do not go through |onFocus| as it
        // contains a lot of logic that can move the selection (if in an
        // editable).
        const range = cursors.Range.fromNode(focus);
        new Output()
            .withRichSpeechAndBraille(range, null, OutputEventType.NAVIGATE)
            .go();
        ChromeVoxState.instance.setCurrentRange(range);
      }
    }.bind(this));
  }

  /**
   * Provides all feedback once a scrolled to anchor event fires.
   * @param {!ChromeVoxEvent} evt
   */
  onScrolledToAnchor(evt) {
    if (!evt.target) {
      return;
    }

    if (ChromeVoxState.instance.currentRange) {
      const target = evt.target;
      const current = ChromeVoxState.instance.currentRange.start.node;
      if (AutomationUtil.getTopLevelRoot(current) !==
          AutomationUtil.getTopLevelRoot(target)) {
        // Ignore this event if the root of the target differs from that of the
        // current range.
        return;
      }
    }

    this.onEventDefault(evt);
  }

  /**
   * Create an editable text handler for the given node if needed.
   * @param {!AutomationNode} node
   * @param {boolean=} opt_onFocus True if called within a focus event
   *     handler. False by default.
   * @return {boolean} True if the handler exists (created/already present).
   */
  createTextEditHandlerIfNeeded_(node, opt_onFocus) {
    if (!node.state.editable) {
      return false;
    }

    if (!ChromeVoxState.instance.currentRange ||
        !ChromeVoxState.instance.currentRange.start ||
        !ChromeVoxState.instance.currentRange.start.node) {
      return false;
    }

    const topRoot = AutomationUtil.getTopLevelRoot(node);
    if (!node.state.focused ||
        (topRoot && topRoot.parent && !topRoot.parent.state.focused)) {
      return false;
    }

    // Re-target the node to the root of the editable.
    let target = node;
    target = AutomationUtil.getEditableRoot(target);
    let voxTarget = ChromeVoxState.instance.currentRange.start.node;
    voxTarget = AutomationUtil.getEditableRoot(voxTarget) || voxTarget;

    // It is possible that ChromeVox has range over some other node when a
    // text field is focused. Only allow this when focus is on a desktop node,
    // ChromeVox is over the keyboard, or during focus events.
    if (!target || !voxTarget ||
        (!opt_onFocus && target !== voxTarget &&
         target.root.role !== RoleType.DESKTOP &&
         voxTarget.root.role !== RoleType.DESKTOP &&
         !AutomationUtil.isDescendantOf(target, voxTarget) &&
         !AutomationUtil.getAncestors(voxTarget.root)
              .find((n) => n.role === RoleType.KEYBOARD))) {
      return false;
    }

    if (!this.textEditHandler_ || this.textEditHandler_.node !== target) {
      this.textEditHandler_ = editing.TextEditHandler.createForNode(target);
    }

    return !!this.textEditHandler_;
  }

  /**
   * @param {ChromeVoxEvent} evt
   * @private
   */
  maybeRecoverFocusAndOutput_(evt, focus) {
    const focusedRoot = AutomationUtil.getTopLevelRoot(focus);
    if (!focusedRoot) {
      return;
    }

    let curRoot;
    if (ChromeVoxState.instance.currentRange) {
      curRoot = AutomationUtil.getTopLevelRoot(
          ChromeVoxState.instance.currentRange.start.node);
    }

    // If initial focus was already placed inside this page (e.g. if a user
    // starts tabbing before load complete), then don't move ChromeVox's
    // position on the page.
    if (curRoot && focusedRoot === curRoot &&
        this.lastRootUrl_ === focusedRoot.docUrl) {
      return;
    }

    this.lastRootUrl_ = focusedRoot.docUrl || '';
    const o = new Output();
    // Restore to previous position.
    let url = focusedRoot.docUrl;
    url = url.substring(0, url.indexOf('#')) || url;
    const pos = ChromeVox.position[url];

    // Deny recovery for chrome urls.
    if (pos && url.indexOf('chrome://') !== 0) {
      focusedRoot.hitTestWithReply(
          pos.x, pos.y, this.onHitTestResult.bind(this));
      return;
    }

    // If range is already on |focus|, exit early to prevent duplicating output.
    const currentRange = ChromeVoxState.instance.currentRange;
    if (currentRange && currentRange.start && currentRange.start.node &&
        currentRange.start.node === focus) {
      return;
    }

    // This catches initial focus (i.e. on startup).
    if (!curRoot && focus !== focusedRoot) {
      o.format('$name', focusedRoot);
    }

    ChromeVoxState.instance.setCurrentRange(cursors.Range.fromNode(focus));

    o.withRichSpeechAndBraille(
         ChromeVoxState.instance.currentRange, null, evt.type)
        .go();
  }

  /**
   * Initializes global state for DesktopAutomationHandler.
   */
  static init() {
    if (DesktopAutomationHandler.instance) {
      throw new Error('DesktopAutomationHandler.instance already exists.');
    }

    chrome.automation.getDesktop(function(desktop) {
      DesktopAutomationHandler.instance = new DesktopAutomationHandler(desktop);
    });
  }
};

/**
 * Time to wait until processing more value changed events.
 * @const {number}
 */
DesktopAutomationHandler.MIN_VALUE_CHANGE_DELAY_MS = 50;

/**
 * Time to wait until processing more alert events with the same text content.
 * @const {number}
 */
DesktopAutomationHandler.MIN_ALERT_DELAY_MS = 50;

/**
 * Time to wait until processing more live region change events on the same
 * text content.
 * @const {number}
 */
DesktopAutomationHandler.LIVE_REGION_DELAY_MS = 100;

/**
 * Time to wait before announcing attribute changes that are otherwise too
 * disruptive.
 * @const {number}
 */
DesktopAutomationHandler.ATTRIBUTE_DELAY_MS = 1500;

/**
 * Controls announcement of non-user-initiated events.
 * @type {boolean}
 */
DesktopAutomationHandler.announceActions = false;

/**
 * Global instance.
 * @type {DesktopAutomationHandler}
 */
DesktopAutomationHandler.instance;
});  // goog.scope
