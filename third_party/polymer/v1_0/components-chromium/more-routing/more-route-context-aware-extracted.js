MoreRouting.ContextAware = {

    /** @override */
    ready: function() {
      this._makeRoutingReady();
    },

    /**
     * Calls `routingReady`, and ensures that it is called in a top-down manner.
     *
     * We need to be sure that parent nodes have `routingReady` triggered before
     * their children so that they can properly configure nested routes.
     *
     * Unfortunately, `ready` is sometimes bottom-up, sometimes top-down.
     * Ideally, this wouldn't be necessary.
     *
     * @see https://github.com/Polymer/polymer/pull/1448
     */
    _makeRoutingReady: function() {
      if (this.routingIsReady) return;

      var node = this;
      while (node = Polymer.dom(node).parentNode) {
        if (typeof node._makeRoutingReady === 'function') break;
      }
      if (node) node._makeRoutingReady();

      this.parentRoute = this._findParentRoute();
      this.routingIsReady = true;
      if (typeof this.routingReady === 'function') this.routingReady();
    },

    _findParentRoute: function() {
      var node = this;
      while (node) {
        node = Polymer.dom(node).parentNode;
        if (node && node.nodeType !== Node.ELEMENT_NODE) {
          node = node.host;
        }

        var route = node && node.moreRouteContext;
        if (route instanceof MoreRouting.Route) {
          return route;
        }
      }
      return null;
    },

  };