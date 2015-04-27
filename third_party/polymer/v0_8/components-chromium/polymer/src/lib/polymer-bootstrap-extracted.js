

  // until ES6 modules become standard, we follow Occam and simply stake out 
  // a global namespace

  // Polymer is a Function, but of course this is also an Object, so we 
  // hang various other objects off of Polymer.*
  (function() {
    var userPolymer = window.Polymer;
    
    window.Polymer = function(prototype) {
      var ctor = desugar(prototype);
      // native Custom Elements treats 'undefined' extends property
      // as valued, the property must not exist to be ignored
      var options = {
        prototype: ctor.prototype
      };
      if (prototype.extends) {
        options.extends = prototype.extends;
      }
      Polymer.telemetry._registrate(prototype);
      document.registerElement(prototype.is, options);
      return ctor;
    };

    var desugar = function(prototype) {
      prototype = Polymer.Base.chainObject(prototype, Polymer.Base);
      prototype.registerCallback();
      return prototype.constructor;
    };

    window.Polymer = Polymer;

    if (userPolymer) {
      for (var i in userPolymer) {
        Polymer[i] = userPolymer[i];
      }
    }

    Polymer.Class = desugar;

  })();
  /*
  // Raw usage
  [ctor =] Polymer.Class(prototype);
  document.registerElement(name, ctor);
  
  // Simplified usage
  [ctor = ] Polymer(prototype);
  */

  // telemetry: statistics, logging, and debug

  Polymer.telemetry = {
    registrations: [],
    _regLog: function(prototype) {
      console.log('[' + prototype.is + ']: registered')
    },
    _registrate: function(prototype) {
      this.registrations.push(prototype);
      Polymer.log && this._regLog(prototype);
    },
    dumpRegistrations: function() {
      this.registrations.forEach(this._regLog);
    }
  };

