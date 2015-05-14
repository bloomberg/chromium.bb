

  (function() {

    'use strict';

    function classNames(obj) {
      var classNames = [];
      for (var key in obj) {
        if (obj.hasOwnProperty(key) && obj[key]) {
          classNames.push(key);
        }
      }

      return classNames.join(' ');
    }

    Polymer({

      is: 'paper-toolbar',

      enableCustomStyleProperties: true,

      properties: {

        /**
         * Controls how the items are aligned horizontally when they are placed
         * at the bottom.
         * Options are `start`, `center`, `end`, `justified` and `around`.
         *
         * @attribute bottomJustify
         * @type string
         * @default ''
         */
        bottomJustify: {
          type: String,
          value: ''
        },

        /**
         * Controls how the items are aligned horizontally.
         * Options are `start`, `center`, `end`, `justified` and `around`.
         *
         * @attribute justify
         * @type string
         * @default ''
         */
        justify: {
          type: String,
          value: ''
        },

        /**
         * Controls how the items are aligned horizontally when they are placed
         * in the middle.
         * Options are `start`, `center`, `end`, `justified` and `around`.
         *
         * @attribute middleJustify
         * @type string
         * @default ''
         */
        middleJustify: {
          type: String,
          value: ''
        }

      },

      _computeBarClassName: function(barJustify) {
        var classObj = {
          center: true,
          horizontal: true,
          layout: true,
          'toolbar-tools': true
        };

        // If a blank string or any falsy value is given, no other class name is
        // added.
        if (barJustify) {
          var justifyClassName = (barJustify === 'justified') ?
              barJustify :
              barJustify + '-justified';

          classObj[justifyClassName] = true;
        }

        return classNames(classObj);
      }

    });

  }());

