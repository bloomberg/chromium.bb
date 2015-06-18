

  Polymer({
    is: 'iron-localstorage',

    /**
     * Fired when value loads from localStorage.
     *
     * @event paper-responsive-change
     * @param {{externalChange: boolean}} detail -
     *     externalChange: True if change occured in different window.
     */

    /**
     * Fired when loaded value is null.
     * You can use event handler to initialize default value.
     *
     * @event iron-localstorage-load-empty
     */
    properties: {
      /**
       * The key to the data stored in localStorage.
       */
      name: {
        type: String,
        value: ''
      },
      /**
       * The data associated with this storage.
       * If value is set to null, and storage is in useRaw mode, item will be deleted
       * @type {*}
       */
      value: {
        type: Object,
        notify: true
      },

      /**
       * Value is stored and retrieved without JSON parse if true
       */
      useRaw: {
        type: Boolean,
        value: false
      },

      /**
       * Auto save is disabled if true. Default to false.
       */
      autoSaveDisabled: {
        type: Boolean,
        value: false
      },
      /**
       * Last error encountered while saving/loading items. Null otherwise
       */
      errorMessage: {
        type: String,
        notify: true
      },
      /*
       * True if value was loaded
       */
      _loaded: {
        type: Boolean,
        value: false
      }
    },

    observers: [
      'reload(name,useRaw)',
      '_trySaveValue(value, _loaded, autoSaveDisabled)'
    ],

    ready: function() {
      this._boundHandleStorage = this._handleStorage.bind(this);
    },

    attached: function() {
      window.addEventListener('storage', this._boundHandleStorage);
    },

    detached: function() {
      window.removeEventListener('storage', this._boundHandleStorage);
    },

    _handleStorage: function(ev) {
      if (ev.key == this.name) {
        this._load(true);
      }
    },

    _trySaveValue: function(value, _loaded, autoSaveDisabled) {
      if (this._justLoaded) { // guard against saving after _load()
        this._justLoaded = false;
        return;
      }
      if (_loaded && !autoSaveDisabled) {
        this.save();
      }
    },

    /**
     * Loads the value again. Use if you modify
     * localStorage using DOM calls, and want to
     * keep this element in sync.
     */
    reload: function() {
      this._load();
    },

    /**
     * loads value from local storage
     * @param {boolean=} externalChange true if loading changes from a different window
     */
    _load: function(externalChange) {
      var v = window.localStorage.getItem(this.name);

      if (v === null) {
        this.fire('iron-localstorage-load-empty');
      } else if (!this.useRaw) {
        try {
          v = JSON.parse(v);
        } catch(x) {
          this.errorMessage = "Could not parse local storage value";
          console.error("could not parse local storage value", v);
        }
      }

      this._justLoaded = true;
      this._loaded = true;
      this.value = v;
      this.fire('iron-localstorage-load', { externalChange: externalChange});
    },

    /**
     * Saves the value to localStorage. Call to save if autoSaveDisabled is set.
     * If `value` is null, deletes localStorage.
     */
    save: function() {
      var v = this.useRaw ? this.value : JSON.stringify(this.value);
      try {
        if (this.value === null) {
          window.localStorage.removeItem(this.name);
        } else {
          window.localStorage.setItem(this.name, /** @type {string} */ (v));
        }
      }
      catch(ex) {
        // Happens in Safari incognito mode,
        this.errorMessage = ex.message;
        console.error("localStorage could not be saved. Safari incoginito mode?", ex);
      }
    }

  });

