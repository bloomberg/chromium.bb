importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  innerframe = document.getElementById("innerframe")
  return mouseClickInTarget('#target0').then(function() {
    return  mouseMoveIntoTarget('#target0').then(function() {
      return  mouseMoveIntoTarget('#target1', innerframe).then(function() {
        return  mouseClickInTarget('#target1', innerframe).then(function() {
          return  mouseMoveIntoTarget('#target1', innerframe).then(function() {
            return  mouseMoveIntoTarget('#target0');
          });
        });
      });
    });
  });
}