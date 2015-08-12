Polymer({

    is: 'more-routing-config',

    properties: {

      /**
       * The routing driver to use.
       *
       *  * `hash`: `MoreRouting.HashDriver`
       *  * `path`: `MoreRouting.PathDriver`
       *  * `mock`: `MoreRouting.MockDriver`
       *
       */
      driver: String,

      /**
       *
       */
      urlPrefix: String,

    },

    ready: function() {
      var config = {};
      if (this.urlPrefix) config.prefix = this.urlPrefix;

      var driver;
      // TODO(nevir): Support custom drivers, too.
      if (this.driver === 'hash') {
        driver = new MoreRouting.HashDriver(config);
      } else if (this.driver === 'path') {
        driver = new MoreRouting.PathDriver(config);
      } else if (this.driver === 'mock') {
        driver = new MoreRouting.MockDriver(config);
      } else {
        throw new Error('Unknown driver type "' + this.driver + '"');
      }

      MoreRouting.driver = driver;
    },

  });