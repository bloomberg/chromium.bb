

  Polymer('paper-toggle-button', {
    
    /**
     * Fired when the checked state changes due to user interaction.
     *
     * @event change
     */
     
    /**
     * Fired when the checked state changes.
     *
     * @event core-change
     */

    /**
     * Gets or sets the state, `true` is checked and `false` is unchecked.
     *
     * @attribute checked
     * @type boolean
     * @default false
     */
    checked: false,

    /**
     * If true, the toggle button is disabled.  A disabled toggle button cannot
     * be tapped or dragged to change the checked state.
     *
     * @attribute disabled
     * @type boolean
     * @default false
     */
    disabled: false,

    eventDelegates: {
      down: 'downAction',
      up: 'upAction',
      tap: 'tap',
      trackstart: 'trackStart',
      trackx: 'trackx',
      trackend: 'trackEnd'
    },

    downAction: function(e) {
      var rect = this.$.ink.getBoundingClientRect();
      this.$.ink.downAction({
        x: rect.left + rect.width / 2,
        y: rect.top + rect.height / 2
      });
    },

    upAction: function(e) {
      this.$.ink.upAction();
    },

    tap: function() {
      if (this.disabled) {
        return;
      }
      this.checked = !this.checked;
      this.fire('change');
    },

    trackStart: function(e) {
      if (this.disabled) {
        return;
      }
      this._w = this.$.toggleBar.offsetWidth / 2;
      e.preventTap();
    },

    trackx: function(e) {
      this._x = Math.min(this._w, 
          Math.max(0, this.checked ? this._w + e.dx : e.dx));
      this.$.toggleButton.classList.add('dragging');
      var s =  this.$.toggleButton.style;
      s.webkitTransform = s.transform = 'translate3d(' + this._x + 'px,0,0)';
    },

    trackEnd: function() {
      var s =  this.$.toggleButton.style;
      s.transform = s.webkitTransform = '';
      this.$.toggleButton.classList.remove('dragging');
      var old = this.checked;
      this.checked = Math.abs(this._x) > this._w / 2;
      if (this.checked !== old) {
        this.fire('change');
      }
    },
    
    checkedChanged: function() {
      this.setAttribute('aria-pressed', Boolean(this.checked));
      this.fire('core-change');
    }

  });

