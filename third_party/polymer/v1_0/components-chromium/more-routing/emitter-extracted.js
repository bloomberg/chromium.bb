(function(scope) {
var MoreRouting = scope.MoreRouting = scope.MoreRouting || {};
MoreRouting.Emitter = Object.create(null);  // Minimal set of properties.

/**
 * A dumb prototype that provides very simple event subscription.
 *
 * You are responsible for initializing `__listeners` as an array on objects
 * that make use of this.
 */
Object.defineProperties(MoreRouting.Emitter, {

  /**
   * Registers a callback that will be called each time any parameter managed by
   * this object (or its parents) have changed.
   *
   * @param {!Function} callback
   * @return {{close: function()}}
   */
  __subscribe: {
    value: function __subscribe(callback) {
      this.__listeners.push(callback);

      return {
        close: this.__unsubscribe.bind(this, callback),
      };
    },
  },

  /**
   * Unsubscribes a previously registered callback.
   *
   * @param {!Function} callback
   */
  __unsubscribe: {
    value: function __unsubscribe(callback) {
      var index = this.__listeners.indexOf(callback);
      if (index < 0) {
        console.warn(this, 'attempted unsubscribe of unregistered listener:', callback);
        return;
      }
      this.__listeners.splice(index, 1);
    },
  },

  /**
   * Notifies subscribed callbacks.
   */
  __notify: {
    value: function __notify(key, value) {
      if (this.__silent) return;
      var args = Array.prototype.slice.call(arguments);
      // Notify listeners on parents first.
      var parent = Object.getPrototypeOf(this);
      if (parent && parent.__notify && parent.__listeners) {
        parent.__notify.apply(parent, args);
      }

      this.__listeners.forEach(function(listener) {
        listener.apply(null, args);
      }.bind(this));
    },
  },

});

})(window);