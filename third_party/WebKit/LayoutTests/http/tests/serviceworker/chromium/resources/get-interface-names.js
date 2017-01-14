function get_interface_names(global_object, interface_names) {
  var result = [];
  function collect_property_info(object, property_name, output) {
    var descriptor = Object.getOwnPropertyDescriptor(object, property_name);
    if ('value' in descriptor) {
      if (typeof descriptor.value === 'function') {
        output.push(' method ' + property_name);
      } else {
        output.push(' attribute ' + property_name);
      }
    } else {
      if (descriptor.get) {
        output.push(' getter ' + property_name);
      }
      if (descriptor.set) {
        output.push(' setter ' + property_name);
      }
    }
  }
  interface_names.sort();
  interface_names.forEach(function(interface_name) {
    if (this[interface_name] === undefined) {
      return;
    }
    result.push('interface ' + interface_name);
    var property_names =
        Object.getOwnPropertyNames(this[interface_name].prototype);
    var property_strings = [];
    property_names.forEach(function(property_name) {
      collect_property_info(this[interface_name].prototype,
                            property_name,
                            property_strings);
    });
    result.push.apply(result, property_strings.sort());
  });
  return result.join("\n");;
}
