

  Polymer.EventApi = (function() {

    var Settings = Polymer.Settings;

    var EventApi = function(event) {
      this.event = event;
    };

    if (Settings.useShadow) {

      EventApi.prototype = {
        
        get rootTarget() {
          return this.event.path[0];
        },

        get localTarget() {
          return this.event.target;
        },

        get path() {
          return this.event.path;
        }

      };

    } else {

      EventApi.prototype = {
      
        get rootTarget() {
          return this.event.target;
        },

        get localTarget() {
          var current = this.event.currentTarget;
          var currentRoot = current && Polymer.dom(current).getOwnerRoot();
          var p$ = this.path;
          for (var i=0; i < p$.length; i++) {
            if (Polymer.dom(p$[i]).getOwnerRoot() === currentRoot) {
              return p$[i];
            }
          }
        },

        // TODO(sorvell): simulate event.path. This probably incorrect for
        // non-bubbling events.
        get path() {
          if (!this.event._path) {
            var path = [];
            var o = this.rootTarget;
            while (o) {
              path.push(o);
              o = Polymer.dom(o).parentNode || o.host;
            }
            // event path includes window in most recent native implementations
            path.push(window);
            this.event._path = path;
          }
          return this.event._path;
        }

      };

    }

    var factory = function(event) {
      if (!event.__eventApi) {
        event.__eventApi = new EventApi(event);
      }
      return event.__eventApi;
    };

    return {
      factory: factory
    };

  })();

