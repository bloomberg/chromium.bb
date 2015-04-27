

  /**
   * Supports `listeners` and `keyPresses` objects.
   *
   * Example:
   *
   *     using('Base', function(Base) {
   *
   *       Polymer({
   *
   *         listeners: {
   *           // `click` events on the host are delegated to `clickHandler`
   *           'click': 'clickHandler'
   *         },
   *
   *         keyPresses: {
   *           // 'ESC' key presses are delegated to `escHandler`
   *           Base.ESC_KEY: 'escHandler'
   *         },
   *
   *         ...
   *
   *       });
   *
   *     });
   *
   * @class standard feature: events
   *
   */

  Polymer.Base._addFeature({

    listeners: {},

    _listenListeners: function(listeners) {
      var node, name, key;
      for (key in listeners) {
        if (key.indexOf('.') < 0) {
          node = this;
          name = key;
        } else {
          name = key.split('.');
          node = this.$[name[0]];
          name = name[1];
        }
        this.listen(node, name, listeners[key]);
      }
    },

    listen: function(node, eventName, methodName) {
      var host = this;
      var handler = function(e) {
        if (host[methodName]) {
          host[methodName](e, e.detail);
        } else {
          console.warn('[%s].[%s]: event handler [%s] is null in scope (%o)',
            node.localName, eventName, methodName, host);
        }
      };
      switch (eventName) {
        case 'tap':
        case 'track':
          Polymer.Gestures.add(eventName, node, handler);
          break;

        default:
          node.addEventListener(eventName, handler);
          break;
      }
    },

    keyCodes: {
      ESC_KEY: 27,
      ENTER_KEY: 13,
      LEFT: 37,
      UP: 38,
      RIGHT: 39,
      DOWN: 40,
      SPACE: 32
    }

  });

