// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A command is an abstraction of an action a user can do in the
 * UI.
 *
 * When the focus changes in the document for each command a canExecute event
 * is dispatched on the active element. By listening to this event you can
 * enable and disable the command by setting the event.canExecute property.
 *
 * When a command is executed a command event is dispatched on the active
 * element. Note that you should stop the propagation after you have handled the
 * command if there might be other command listeners higher up in the DOM tree.
 */

cr.define('cr.ui', function() {

  /**
   * This is used to identify keyboard shortcuts.
   * @param {string} shortcut The text used to describe the keys for this
   *     keyboard shortcut.
   * @constructor
   */
  function KeyboardShortcut(shortcut) {
    this.useKeyCode_ = false;
    this.mods_ = {};
    shortcut.split('|').forEach((part) => {
      var partLc = part.toLowerCase();
      switch (partLc) {
        case 'alt':
        case 'ctrl':
        case 'meta':
        case 'shift':
          this.mods_[partLc + 'Key'] = true;
          break;
        default:
          if (this.key_)
            throw Error('Invalid shortcut');
          this.key_ = part;
          // For single key alpha shortcuts use event.keyCode rather than
          // event.key to match how chrome handles shortcuts and allow
          // non-english language input to work.
          if (part.match(/^[a-z]$/)) {
            this.useKeyCode_ = true;
            this.keyCode_ = part.toUpperCase().charCodeAt(0);
          }
      }
    });
  }

  KeyboardShortcut.prototype = {
    /**
     * Whether the keyboard shortcut object matches a keyboard event.
     * @param {!Event} e The keyboard event object.
     * @return {boolean} Whether we found a match or not.
     */
    matchesEvent: function(e) {
      if ((this.useKeyCode_ && e.keyCode == this.keyCode_) ||
          e.key == this.key_) {
        // All keyboard modifiers need to match.
        var mods = this.mods_;
        return ['altKey', 'ctrlKey', 'metaKey', 'shiftKey'].every(function(k) {
          return e[k] == !!mods[k];
        });
      }
      return false;
    }
  };

  /**
   * A list of keyboard shortcuts which all perform one command.
   * @param {string} shortcuts Text-based representation of one or more keyboard
   *     shortcuts, separated by spaces.
   * @constructor
   */
  function KeyboardShortcutList(shortcuts) {
    this.shortcuts_ = shortcuts.split(/\s+/).map(function(shortcut) {
      return new KeyboardShortcut(shortcut);
    });
  }

  KeyboardShortcutList.prototype = {
    /**
     * Returns true if any of the keyboard shortcuts in the list matches a
     * keyboard event.
     * @param {!Event} e
     * @return {boolean}
     */
    matchesEvent: function(e) {
      return this.shortcuts_.some(function(keyboardShortcut) {
        return keyboardShortcut.matchesEvent(e);
      });
    },
  };

  /**
   * Creates a new command element.
   * @constructor
   * @extends {HTMLElement}
   */
  var Command = cr.ui.define('command');

  Command.prototype = {
    __proto__: HTMLElement.prototype,

    /**
     * Initializes the command.
     */
    decorate: function() {
      CommandManager.init(assert(this.ownerDocument));

      if (this.hasAttribute('shortcut'))
        this.shortcut = this.getAttribute('shortcut');
    },

    /**
     * Executes the command by dispatching a command event on the given element.
     * If |element| isn't given, the active element is used instead.
     * If the command is {@code disabled} this does nothing.
     * @param {HTMLElement=} opt_element Optional element to dispatch event on.
     */
    execute: function(opt_element) {
      if (this.disabled)
        return;
      var doc = this.ownerDocument;
      if (doc.activeElement) {
        var e = new Event('command', {bubbles: true});
        e.command = this;

        (opt_element || doc.activeElement).dispatchEvent(e);
      }
    },

    /**
     * Call this when there have been changes that might change whether the
     * command can be executed or not.
     * @param {Node=} opt_node Node for which to actuate command state.
     */
    canExecuteChange: function(opt_node) {
      dispatchCanExecuteEvent(
          this, opt_node || this.ownerDocument.activeElement);
    },

    /**
     * The keyboard shortcut that triggers the command. This is a string
     * consisting of a key (as reported by WebKit in keydown) as
     * well as optional key modifiers joinded with a '|'.
     *
     * Multiple keyboard shortcuts can be provided by separating them by
     * whitespace.
     *
     * For example:
     *   "F1"
     *   "Backspace|Meta" for Apple command backspace.
     *   "a|Ctrl" for Control A
     *   "Delete Backspace|Meta" for Delete and Command Backspace
     *
     * @type {string}
     */
    shortcut_: '',
    get shortcut() {
      return this.shortcut_;
    },
    set shortcut(shortcut) {
      var oldShortcut = this.shortcut_;
      if (shortcut !== oldShortcut) {
        this.keyboardShortcuts_ = new KeyboardShortcutList(shortcut);

        // Set this after the keyboardShortcuts_ since that might throw.
        this.shortcut_ = shortcut;
        cr.dispatchPropertyChange(
            this, 'shortcut', this.shortcut_, oldShortcut);
      }
    },

    /**
     * Whether the event object matches the shortcut for this command.
     * @param {!Event} e The key event object.
     * @return {boolean} Whether it matched or not.
     */
    matchesEvent: function(e) {
      if (!this.keyboardShortcuts_)
        return false;
      return this.keyboardShortcuts_.matchesEvent(e);
    },
  };

  /**
   * The label of the command.
   */
  cr.defineProperty(Command, 'label', cr.PropertyKind.ATTR);

  /**
   * Whether the command is disabled or not.
   */
  cr.defineProperty(Command, 'disabled', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the command is hidden or not.
   */
  cr.defineProperty(Command, 'hidden', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the command is checked or not.
   */
  cr.defineProperty(Command, 'checked', cr.PropertyKind.BOOL_ATTR);

  /**
   * The flag that prevents the shortcut text from being displayed on menu.
   *
   * If false, the keyboard shortcut text (eg. "Ctrl+X" for the cut command)
   * is displayed in menu when the command is associated with a menu item.
   * Otherwise, no text is displayed.
   */
  cr.defineProperty(Command, 'hideShortcutText', cr.PropertyKind.BOOL_ATTR);

  /**
   * Dispatches a canExecute event on the target.
   * @param {!cr.ui.Command} command The command that we are testing for.
   * @param {EventTarget} target The target element to dispatch the event on.
   */
  function dispatchCanExecuteEvent(command, target) {
    var e = new CanExecuteEvent(command);
    target.dispatchEvent(e);
    command.disabled = !e.canExecute;
  }

  /**
   * The command managers for different documents.
   */
  var commandManagers = {};

  /**
   * Keeps track of the focused element and updates the commands when the focus
   * changes.
   * @param {!Document} doc The document that we are managing the commands for.
   * @constructor
   */
  function CommandManager(doc) {
    doc.addEventListener('focus', this.handleFocus_.bind(this), true);
    // Make sure we add the listener to the bubbling phase so that elements can
    // prevent the command.
    doc.addEventListener('keydown', this.handleKeyDown_.bind(this), false);
  }

  /**
   * Initializes a command manager for the document as needed.
   * @param {!Document} doc The document to manage the commands for.
   */
  CommandManager.init = function(doc) {
    var uid = cr.getUid(doc);
    if (!(uid in commandManagers)) {
      commandManagers[uid] = new CommandManager(doc);
    }
  };

  CommandManager.prototype = {

    /**
     * Handles focus changes on the document.
     * @param {Event} e The focus event object.
     * @private
     * @suppress {checkTypes}
     * TODO(vitalyp): remove the suppression.
     */
    handleFocus_: function(e) {
      var target = e.target;

      // Ignore focus on a menu button or command item.
      if (target.menu || target.command)
        return;

      var commands = Array.prototype.slice.call(
          target.ownerDocument.querySelectorAll('command'));

      commands.forEach(function(command) {
        dispatchCanExecuteEvent(command, target);
      });
    },

    /**
     * Handles the keydown event and routes it to the right command.
     * @param {!Event} e The keydown event.
     */
    handleKeyDown_: function(e) {
      var target = e.target;
      var commands = Array.prototype.slice.call(
          target.ownerDocument.querySelectorAll('command'));

      for (var i = 0, command; command = commands[i]; i++) {
        if (command.matchesEvent(e)) {
          // When invoking a command via a shortcut, we have to manually check
          // if it can be executed, since focus might not have been changed
          // what would have updated the command's state.
          command.canExecuteChange();

          if (!command.disabled) {
            e.preventDefault();
            // We do not want any other element to handle this.
            e.stopPropagation();
            command.execute();
            return;
          }
        }
      }
    }
  };

  /**
   * The event type used for canExecute events.
   * @param {!cr.ui.Command} command The command that we are evaluating.
   * @extends {Event}
   * @constructor
   * @class
   */
  function CanExecuteEvent(command) {
    var e = new Event('canExecute', {bubbles: true, cancelable: true});
    e.__proto__ = CanExecuteEvent.prototype;
    e.command = command;
    return e;
  }

  CanExecuteEvent.prototype = {
    __proto__: Event.prototype,

    /**
     * The current command
     * @type {cr.ui.Command}
     */
    command: null,

    /**
     * Whether the target can execute the command. Setting this also stops the
     * propagation and prevents the default. Callers can tell if an event has
     * been handled via |this.defaultPrevented|.
     * @type {boolean}
     */
    canExecute_: false,
    get canExecute() {
      return this.canExecute_;
    },
    set canExecute(canExecute) {
      this.canExecute_ = !!canExecute;
      this.stopPropagation();
      this.preventDefault();
    }
  };

  // Export
  return {
    Command: Command,
    CanExecuteEvent: CanExecuteEvent,
    KeyboardShortcutList: KeyboardShortcutList,
  };
});
