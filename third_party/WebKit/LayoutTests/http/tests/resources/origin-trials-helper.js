// This file provides an OriginTrialsHelper object which can be used by
// LayoutTests that are checking members exposed to script by origin trials.
//
// The current available methods are:
// get_interface_names:
//   Report on the existence of the given interface names on the global object,
//   listing all the properties found for each interface. The properties can be
//   filtered by providing a list of desired property names. As well, it can
//   report on properties of the global object itself, by giving 'global' as one
//   of the interface names.
// Example:
//   OriginTrialsHelper.get_interface_names(
//     this,
//     ['ForeignFetchEvent', 'InstallEvent', 'global'],
//     {'InstallEvent':['registerForeignFetch'], 'global':['onforeignfetch']}));
//
// check_interfaces_in_sw:
//   Collects the results of calling get_interface_names() in a service worker.
//   Use in a promise test to output the results.
// Example:
//   promise_test(t => {
//     var script = 'path/to/script/calling/get_interface_names()'
//     var scope = 'matching scope'
//     return OriginTrialsHelper.check_interfaces_in_sw(t, script, scope)
//        .then(message => {
//          console.log('Interfaces in Service Worker - origin trial enabled.\n'
//                      + message);
//        });
//
// add_token:
//   Adds a trial token to the document, to enable a trial via script
// Example:
//   OriginTrialsHelper.add_token('token produced by generate_token.py');
'use strict';

var OriginTrialsHelper = (function() {
  return {
    get_interface_names:
        (global_object, interface_names,
         opt_property_filters) => {
          var property_filters = opt_property_filters || {};
          var result = [];
          function collect_property_info(object, property_name, output) {
            var descriptor =
                Object.getOwnPropertyDescriptor(object, property_name);
            if ('value' in descriptor) {
              if (typeof descriptor.value === 'function') {
                output.push('    method ' + property_name);
              } else {
                output.push('    attribute ' + property_name);
              }
            } else {
              if (descriptor.get) {
                output.push('    getter ' + property_name);
              }
              if (descriptor.set) {
                output.push('    setter ' + property_name);
              }
            }
          }
          interface_names.sort();
          interface_names.forEach(function(interface_name) {
            var use_global = false;
            var interface_object;
            if (interface_name === 'global') {
              use_global = true;
              interface_object = global_object;
            } else {
              interface_object = global_object[interface_name];
            }
            if (interface_object === undefined) {
              return;
            }
            var interface_prototype;
            var display_name;
            if (use_global) {
              interface_prototype = interface_object;
              display_name = 'global object';
            } else {
              interface_prototype = interface_object.prototype;
              display_name = 'interface ' + interface_name;
            }
            result.push(display_name);
            var property_names =
                Object.getOwnPropertyNames(interface_prototype);
            var match_property_names = property_filters[interface_name];
            if (match_property_names) {
              property_names = property_names.filter(
                  name => { return match_property_names.indexOf(name) >= 0; });
            }
            var property_strings = [];
            property_names.forEach(function(property_name) {
              collect_property_info(
                  interface_prototype, property_name, property_strings);
            });
            result.push.apply(result, property_strings.sort());
          });
          return result.join('\n');
        },

         check_interfaces_in_sw:
             (t, script, scope) => {
               var worker;
               var message;
               var registration;
               return service_worker_unregister_and_register(t, script, scope)
                   .then(reg => {
                     registration = reg;
                     worker = registration.installing;
                     return wait_for_state(t, worker, 'activated');
                   })
                   .then(_ => {
                     var saw_message = new Promise(resolve => {
                       navigator.serviceWorker.onmessage =
                           e => { resolve(e.data); };
                     });
                     worker.postMessage('');
                     return saw_message;
                   })
                   .then(msg => {
                     message = msg;
                     return registration.unregister();
                   })
                   .then(_ => { return message; });
             },

         add_token: (token_string) => {
           var tokenElement = document.createElement('meta');
           tokenElement.httpEquiv = 'origin-trial';
           tokenElement.content = token_string;
           document.head.appendChild(tokenElement);
         }
  }
})();
