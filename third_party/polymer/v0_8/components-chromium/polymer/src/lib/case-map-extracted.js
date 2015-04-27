

  Polymer.CaseMap = {

    _caseMap: {},

    dashToCamelCase: function(dash) {
      var mapped = Polymer.CaseMap._caseMap[dash];
      if (mapped) {
        return mapped;
      }
      // TODO(sjmiles): is rejection test actually helping perf?
      if (dash.indexOf('-') < 0) {
        return Polymer.CaseMap._caseMap[dash] = dash;
      }
      return Polymer.CaseMap._caseMap[dash] = dash.replace(/-([a-z])/g, 
        function(m) {
          return m[1].toUpperCase(); 
        }
      );
    },

    camelToDashCase: function(camel) {
      var mapped = Polymer.CaseMap._caseMap[camel];
      if (mapped) {
        return mapped;
      }
      return Polymer.CaseMap._caseMap[camel] = camel.replace(/([a-z][A-Z])/g, 
        function (g) { 
          return g[0] + '-' + g[1].toLowerCase() 
        }
      );
    }

  };

