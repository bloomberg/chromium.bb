
(function(scope) {

  function withDependencies(task, depends) {
    depends = depends || [];
    if (!depends.map) {
      depends = [depends];
    }
    return task.apply(this, depends.map(marshal));
  }

  function module(name, dependsOrFactory, moduleFactory) {
    var module = null;
    switch (arguments.length) {
      case 0:
        return;
      case 2:
        // dependsOrFactory is `factory` in this case
        module = dependsOrFactory.apply(this);
        break;
      default:
        // dependsOrFactory is `depends` in this case
        module = withDependencies(moduleFactory, dependsOrFactory);
        break;
    }
    modules[name] = module;
  };

  function marshal(name) {
    return modules[name];
  }

  var modules = {};

  var using = function(depends, task) {
    withDependencies(task, depends);
  };

  // exports

  scope.marshal = marshal;
  // `module` confuses commonjs detectors
  scope.modulate = module;
  scope.using = using;

})(this);
