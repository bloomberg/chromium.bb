

  Polymer('paper-autogrow-textarea',{

    publish: {

        /**
         * The textarea that should auto grow.
         *
         * @attribute target
         * @type HTMLTextAreaElement
         * @default null
         */
        target: null,

        /**
         * The initial number of rows.
         *
         * @attribute rows
         * @type number
         * @default 1
         */
        rows: 1,

        /**
         * The maximum number of rows this element can grow to until it
         * scrolls. 0 means no maximum.
         *
         * @attribute maxRows
         * @type number
         * @default 0
         */
        maxRows: 0
    },

    tokens: null,

    observe: {
      rows: 'updateCached',
      maxRows: 'updateCached'
    },

    constrain: function(tokens) {
      var _tokens;
      tokens = tokens || [''];
      // Enforce the min and max heights for a multiline input to avoid measurement
      if (this.maxRows > 0 && tokens.length > this.maxRows) {
        _tokens = tokens.slice(0, this.maxRows);
      } else {
        _tokens = tokens.slice(0);
      }
      while (this.rows > 0 && _tokens.length < this.rows) {
        _tokens.push('');
      }
      return _tokens.join('<br>') + '&nbsp;';
    },

    valueForMirror: function(input) {
      this.tokens = (input && input.value) ? input.value.replace(/&/gm, '&amp;').replace(/"/gm, '&quot;').replace(/'/gm, '&#39;').replace(/</gm, '&lt;').replace(/>/gm, '&gt;').split('\n') : [''];
      return this.constrain(this.tokens);
    },

    /**
     * Sizes this element to fit the input value. This function is automatically called
     * when the user types in new input, but you must call this function if the value
     * is updated imperatively.
     *
     * @method update
     * @param Element The input
     */
    update: function(input) {
      this.$.mirror.innerHTML = this.valueForMirror(input);
    },

    updateCached: function() {
      this.$.mirror.innerHTML = this.constrain(this.tokens);
    },

    inputAction: function(e) {
      this.update(e.target);
    }

  });

