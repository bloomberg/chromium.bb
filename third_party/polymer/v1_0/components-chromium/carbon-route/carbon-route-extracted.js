'use strict';

  Polymer({
    is: 'carbon-route',

    properties: {
      /**
       * The URL component managed by this element.
       */
      route: {
        type: Object,
        notify: true
      },

      /**
       * The pattern of slash-separated segments to match `path` against.
       *
       * For example the pattern "/foo" will match "/foo" or "/foo/bar"
       * but not "/foobar".
       *
       * Path segments like `/:named` are mapped to properties on the `data` object.
       */
      pattern: {
        type: String
      },

      /**
       * The parameterized values that are extracted from the route as
       * described by `pattern`.
       */
      data: {
        type: Object,
        value: function() {return {};},
        notify: true
      },

      /**
       * @type {?Object}
       */
      queryParams: {
        type: Object,
        value: function() {
          return {};
        },
        notify: true
      },

      /**
       * The part of `path` NOT consumed by `pattern`.
       */
      tail: {
        type: Object,
        value: function() {return {path: null, prefix: null, __queryParams: null};},
        notify: true
      },

      active: {
        type: Boolean,
        notify: true,
        readOnly: true
      },

      _skipMatch: {
        type: Boolean,
        value: false
      },
      /**
       * @type {?string}
       */
      _matched: {
        type: String,
        value: ''
      }
    },

    observers: [
      '__tryToMatch(route.path, pattern)',
      '__updatePathOnDataChange(data.*)',
      '__tailPathChanged(tail.path)',
      '__routeQueryParamsChanged(route.__queryParams)',
      '__tailQueryParamsChanged(tail.__queryParams)',
      '__queryParamsChanged(queryParams.*)'
    ],

    created: function() {
      this.linkPaths('route.__queryParams', 'tail.__queryParams');
      this.linkPaths('tail.__queryParams', 'route.__queryParams');
    },

    // IE Object.assign polyfill
    __assign: function(target, source) {
      if (source != null) {
        for (var key in source) {
          target[key] = source[key];
        }
      }

      return target;
    },

    // Deal with the query params object being assigned to wholesale
    __routeQueryParamsChanged: function(queryParams) {
      if (queryParams && this.tail) {
        this.set('tail.__queryParams', queryParams);

        if (!this.active || this._skipMatch) {
          return;
        }

        this._skipMatch = true;

        var qp;

        if (Object.assign) {
          qp = Object.assign({}, queryParams);
        } else {
          qp = this.__assign({}, queryParams);
        }

        this.set('queryParams', qp);
        this._skipMatch = false;
      }
    },

    __tailQueryParamsChanged: function(queryParams) {
      if (queryParams && this.route) {
        this.set('route.__queryParams', queryParams);
      }
    },

    __queryParamsChanged: function(changes) {
      if (!this.active || this._skipMatch) {
        return;
      }

      this.set('route.__' + changes.path, changes.value);
    },

    __resetProperties: function() {
      this._setActive(false);
      this._matched = null;
      //this.tail = { path: null, prefix: null, queryParams: null };
      //this.data = {};
    },

    __tryToMatch: function(path, pattern) {
      if (this._skipMatch || !pattern) {
        return;
      }

      if (!path) {
        this.__resetProperties();
        return;
      }

      var remainingPieces = path.split('/');
      var patternPieces = pattern.split('/');

      var matched = [];
      var namedMatches = {};

      for (var i=0; i < patternPieces.length; i++) {
        var patternPiece = patternPieces[i];
        if (!patternPiece && patternPiece !== '') {
          break;
        }
        var pathPiece = remainingPieces.shift();

        // We don't match this path.
        if (!pathPiece && pathPiece !== '') {
          this.__resetProperties();
          return;
        }
        matched.push(pathPiece);

        if (patternPiece.charAt(0) == ':') {
          namedMatches[patternPiece.slice(1)] = pathPiece;
        } else if (patternPiece !== pathPiece) {
          this.__resetProperties();
          return;
        }
      }

      matched = matched.join('/');
      var tailPath = remainingPieces.join('/');
      if (remainingPieces.length > 0) {
        tailPath = '/' + tailPath;
      }

      this._skipMatch = true;
      this._matched = matched;
      this.data = namedMatches;
      var tailPrefix = this.route.prefix + matched;

      if (!this.tail || this.tail.prefix !== tailPrefix || this.tail.path !== tailPath) {
        this.tail = {
          prefix: tailPrefix,
          path: tailPath,
          __queryParams: this.route.__queryParams
        };
      }
      this._setActive(true);
      this._skipMatch = false;
    },

    __tailPathChanged: function(path) {
      if (!this.active || this._skipMatch) {
        return;
      }
      var newPath = this._matched;
      if (path) {
        if (path.charAt(0) !== '/') {
          path = '/' + path;
        }
        newPath += path;
      }
      this.set('route.path', newPath);
    },

    __updatePathOnDataChange: function() {
      if (!this.route || this._skipMatch || !this.active) {
        return;
      }
      this._skipMatch = true;
      this.tail = {path: null, prefix: null, queryParams: null};
      this.set('route.path', this.__getLink({}));
      this._skipMatch = false;
    },

    __getLink: function(overrideValues) {
      var values = {tail: this.tail};
      for (var key in this.data) {
        values[key] = this.data[key];
      }
      for (var key in overrideValues) {
        values[key] = overrideValues[key];
      }
      var patternPieces = this.pattern.split('/');
      var interp = patternPieces.map(function(value) {
        if (value[0] == ':') {
          value = values[value.slice(1)];
        }
        return value;
      }, this);
      if (values.tail && values.tail.path) {
        interp.push(values.tail.path);
      }
      return interp.join('/');
    }
  });