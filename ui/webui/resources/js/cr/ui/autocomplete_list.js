// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var List = cr.ui.List;
  /** @const */ var ListItem = cr.ui.ListItem;

  /**
   * Creates a new autocomplete list item.
   * @param {Object} pageInfo The page this item represents.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function AutocompleteListItem(pageInfo) {
    var el = cr.doc.createElement('div');
    el.pageInfo_ = pageInfo;
    AutocompleteListItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as an autocomplete list item.
   * @param {!HTMLElement} el The element to decorate.
   */
  AutocompleteListItem.decorate = function(el) {
    el.__proto__ = AutocompleteListItem.prototype;
    el.decorate();
  };

  AutocompleteListItem.prototype = {
    __proto__: ListItem.prototype,

    /** @override */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      var title = this.pageInfo_['title'];
      var url = this.pageInfo_['displayURL'];
      var titleEl = this.ownerDocument.createElement('span');
      titleEl.className = 'title';
      titleEl.textContent = title || url;
      this.appendChild(titleEl);

      if (title && title.length > 0 && url != title) {
        var separatorEl = this.ownerDocument.createTextNode(' - ');
        this.appendChild(separatorEl);

        var urlEl = this.ownerDocument.createElement('span');
        urlEl.className = 'url';
        urlEl.textContent = url;
        this.appendChild(urlEl);
      }
    },
  };

  /**
   * Creates a new autocomplete list popup.
   * @constructor
   * @extends {cr.ui.List}
   */
  var AutocompleteList = cr.ui.define('list');

  AutocompleteList.prototype = {
    __proto__: List.prototype,

    /**
     * The text field the autocomplete popup is currently attached to, if any.
     * @type {HTMLElement}
     * @private
     */
    targetInput_: null,

    /**
     * Keydown event listener to attach to a text field.
     * @type {Function}
     * @private
     */
    textFieldKeyHandler_: null,

    /**
     * Input event listener to attach to a text field.
     * @type {Function}
     * @private
     */
    textFieldInputHandler_: null,

    /**
     * A function to call when new suggestions are needed.
     * @type {Function}
     * @private
     */
    suggestionUpdateRequestCallback_: null,

    /** @override */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.classList.add('autocomplete-suggestions');
      this.selectionModel = new cr.ui.ListSingleSelectionModel;

      this.textFieldKeyHandler_ = this.handleAutocompleteKeydown_.bind(this);
      var self = this;
      this.textFieldInputHandler_ = function(e) {
        if (self.suggestionUpdateRequestCallback_)
          self.suggestionUpdateRequestCallback_(self.targetInput_.value);
      };
      this.addEventListener('change', function(e) {
        var input = self.targetInput;
        if (!input || !self.selectedItem)
          return;
        input.value = self.selectedItem['url'];
        // Programatically change the value won't trigger a change event, but
        // clients are likely to want to know when changes happen, so fire one.
        var changeEvent = document.createEvent('Event');
        changeEvent.initEvent('change', true, true);
        input.dispatchEvent(changeEvent);
      });
      // Start hidden; adding suggestions will unhide.
      this.hidden = true;
    },

    /** @override */
    createItem: function(pageInfo) {
      return new AutocompleteListItem(pageInfo);
    },

    /**
     * The suggestions to show.
     * @type {Array}
     */
    set suggestions(suggestions) {
      this.dataModel = new ArrayDataModel(suggestions);
      this.hidden = !this.targetInput_ || suggestions.length == 0;
    },

    /**
     * A function to call when the attached input field's contents change.
     * The function should take one string argument, which will be the text
     * to autocomplete from.
     * @type {Function}
     */
    set suggestionUpdateRequestCallback(callback) {
      this.suggestionUpdateRequestCallback_ = callback;
    },

    /**
     * Attaches the popup to the given input element. Requires
     * that the input be wrapped in a block-level container of the same width.
     * @param {HTMLElement} input The input element to attach to.
     */
    attachToInput: function(input) {
      if (this.targetInput_ == input)
        return;

      this.detach();
      this.targetInput_ = input;
      this.style.width = input.getBoundingClientRect().width + 'px';
      this.hidden = false;  // Necessary for positionPopupAroundElement to work.
      cr.ui.positionPopupAroundElement(input, this, cr.ui.AnchorType.BELOW);
      // Start hidden; when the data model gets results the list will show.
      this.hidden = true;

      input.addEventListener('keydown', this.textFieldKeyHandler_, true);
      input.addEventListener('input', this.textFieldInputHandler_);
    },

    /**
     * Detaches the autocomplete popup from its current input element, if any.
     */
    detach: function() {
      var input = this.targetInput_;
      if (!input)
        return;

      input.removeEventListener('keydown', this.textFieldKeyHandler_, true);
      input.removeEventListener('input', this.textFieldInputHandler_);
      this.targetInput_ = null;
      this.suggestions = [];
    },

    /**
     * Makes sure that the suggestion list matches the width of the input it is.
     * attached to. Should be called any time the input is resized.
     */
    syncWidthToInput: function() {
      var input = this.targetInput_;
      if (input)
        this.style.width = input.getBoundingClientRect().width + 'px';
    },

    /**
     * @return {HTMLElement} The text field the autocomplete popup is currently
     *     attached to, if any.
     */
    get targetInput() {
      return this.targetInput_;
    },

    /**
     * Handles input field key events that should be interpreted as autocomplete
     * commands.
     * @param {Event} event The keydown event.
     * @private
     */
    handleAutocompleteKeydown_: function(event) {
      if (this.hidden)
        return;
      var handled = false;
      switch (event.keyIdentifier) {
        case 'U+001B':  // Esc
          this.suggestions = [];
          handled = true;
          break;
        case 'Enter':
          // If the user has already selected an item using the arrow keys then
          // presses Enter, clear the suggestions but also keep
          // |handled| = false, so the input field can handle the event as well.
          this.suggestions = [];
          break;
        case 'Up':
        case 'Down':
          var newEvent = new cr.Event(event.type);
          newEvent.keyIdentifier = event.keyIdentifier;
          this.dispatchEvent(newEvent);
          handled = true;
          break;
      }
      // Don't let arrow keys affect the text field, or bubble up to, e.g.,
      // an enclosing list item.
      if (handled) {
        event.preventDefault();
        event.stopPropagation();
      }
    },
  };

  return {
    AutocompleteList: AutocompleteList
  };
});
