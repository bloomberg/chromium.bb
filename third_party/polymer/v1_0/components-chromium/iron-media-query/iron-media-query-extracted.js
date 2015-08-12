Polymer({

    is: 'iron-media-query',

    properties: {

      /**
       * The Boolean return value of the media query.
       */
      queryMatches: {
        type: Boolean,
        value: false,
        readOnly: true,
        notify: true
      },

      /**
       * The CSS media query to evaluate.
       */
      query: {
        type: String,
        observer: 'queryChanged'
      }

    },

    created: function() {
      this._mqHandler = this.queryHandler.bind(this);
    },

    queryChanged: function(query) {
      if (this._mq) {
        this._mq.removeListener(this._mqHandler);
      }
      if (query[0] !== '(') {
        query = '(' + query + ')';
      }
      this._mq = window.matchMedia(query);
      this._mq.addListener(this._mqHandler);
      this.queryHandler(this._mq);
    },

    queryHandler: function(mq) {
      this._setQueryMatches(mq.matches);
    }

  });