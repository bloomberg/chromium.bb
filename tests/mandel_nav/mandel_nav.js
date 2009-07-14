var canvasWidth;
var plugin;
var nav;

// Utility functions to get and set the value of user input fields.
var getIntValue = function(id) {
  var element = document.getElementById(id);
  var value = element.value;
  return parseInt(value);
}

var getFloatValue = function(id) {
  var element = document.getElementById(id);
  var value = element.value;
  return parseFloat(value);
}

var setValue = function(id, val) {
  var element = document.getElementById(id);
  element.value = val;
}


// All navigation is performed through NavigationClass methods.
// These are responsible for pan-and-zoom control.
var NavigationClass = function(mandel_init) {
  var xmin;
  var xmax;
  var ymin;
  var ymax;
  var step = 0.02;
  var timeStep = 33;
  this.mandel = mandel_init;
  this.currentlyMoving = 0;

  // Sets the range of the display to the default values.
  this.setDefaults = function() {
    setValue('xleft',   -3.0);
    setValue('xright',   1.0);
    setValue('ytop',     2.0);
    setValue('ybottom', -2.0);
    this.mandel.timedDraw();
  }

  // Reads the range coordinates from the HTML input boxes.
  this.getCoords = function() {
    xmin = getFloatValue('xleft');
    xmax = getFloatValue('xright');
    ymin = getFloatValue('ybottom');
    ymax = getFloatValue('ytop');
  }

  // Zoom in on a centered region (1.0 - step) times smaller than the current.
  this.zoomIn = function() {
    if (this.currentlyMoving) {
      return;
    }
    this.currentlyMoving = 1;
    this.getCoords();
    var xsize = (xmax - xmin);
    setValue('xright', xmax - xsize * step / 2.0);
    setValue('xleft', xmin + xsize * step / 2.0);
    var ysize = (ymax - ymin);
    setValue('ytop', ymax - ysize * step / 2.0);
    setValue('ybottom', ymin + ysize * step / 2.0);
    this.mandel.timedDraw();
    this.currentlyMoving = 0;
  }

  // Zoom out to a centered region step / (1.0 - step) times larger than the
  // current.
  this.zoomOut = function() {
    if (this.currentlyMoving) {
      return;
    }
    this.currentlyMoving = 1;
    this.getCoords();
    var scale = step / (1.0 - step);
    var xsize = (xmax - xmin);
    setValue('xright', xmax + xsize * scale / 2.0);
    setValue('xleft', xmin - xsize * scale / 2.0);
    var ysize = (ymax - ymin);
    setValue('ytop', ymax + ysize * scale / 2.0);
    setValue('ybottom', ymin - ysize * scale / 2.0);
    this.mandel.timedDraw();
    this.currentlyMoving = 0;
  }

  // Move the viewable range up by step * ysize.
  this.moveUp = function() {
    if (this.currentlyMoving) {
      return;
    }
    currentlyMoving = 1;
    this.getCoords();
    var ysize = (ymax - ymin);
    setValue('ytop', ymax + ysize * step);
    setValue('ybottom', ymin + ysize * step);
    this.mandel.timedDraw();
    this.currentlyMoving = 0;
  }

  // Move the viewable range down by step * ysize.
  this.moveDown = function() {
    if (this.currentlyMoving) {
      return;
    }
    this.currentlyMoving = 1;
    this.getCoords();
    var ysize = (ymax - ymin);
    setValue('ytop', ymax - ysize * step);
    setValue('ybottom', ymin - ysize * step);
    this.mandel.timedDraw();
    this.currentlyMoving = 0;
  }

  // Move the viewable range left by step * xsize.
  this.moveLeft = function() {
    if (this.currentlyMoving) {
      return;
    }
    this.currentlyMoving = 1;
    this.getCoords();
    var xsize = (xmax - xmin);
    setValue('xleft', xmin - xsize * step);
    setValue('xright', xmax - xsize * step);
    this.mandel.timedDraw();
    this.currentlyMoving = 0;
  }

  // Move the viewable range right by step * xsize.
  this.moveRight = function() {
    if (this.currentlyMoving) {
      return;
    }
    this.currentlyMoving = 1;
    this.getCoords();
    var xsize = (xmax - xmin);
    setValue('xleft', xmin + xsize * step);
    setValue('xright', xmax + xsize * step);
    this.mandel.timedDraw();
    this.currentlyMoving = 0;
  }

  // All the repX functions are activated when a button is pressed, providing
  // auto-repeat.
  this.repZoomIn = function(elt) {
    elt.intervalTimer_ = setInterval(function() { nav.zoomIn(); }, timeStep);
  }

  this.repZoomOut = function(elt) {
    elt.intervalTimer_ = setInterval(function() { nav.zoomOut(); }, timeStep);
  }

  this.repMoveUp = function(elt) {
    elt.intervalTimer_ = setInterval(function() { nav.moveUp(); }, timeStep);
  }

  this.repMoveDown = function(elt) {
    elt.intervalTimer_ = setInterval(function() { nav.moveDown(); }, timeStep);
  }

  this.repMoveLeft = function(elt) {
    elt.intervalTimer_ = setInterval(function() { nav.moveLeft(); }, timeStep);
  }

  this.repMoveRight = function(elt) {
    elt.intervalTimer_ = setInterval(function() { nav.moveRight(); }, timeStep);
  }

  this.stopInterval = function(elt) {
    clearInterval(elt.intervalTimer_);
  }
};

// NaClMandel encapsulates the Native Client module that computes the
// Mandelbrot set.  It provides a function to draw a given range of the
// image, which is implemented to attempt to avoid recomputation as much as
// possible.
var NaClMandel = function(module) {
  // Record the NativeClient module that was returned so that we can
  // invoke methods on it.
  this.naclModule_ = module;
  // Initialize colorShifting_ to be off (not "psychedelic mode").
  this.colorShifting_ = null;
  // Set the default colors.
  this.naclModule_.shift_colors(0);

  // Compute the NativeClient mandelbrot picture and render into canvas.
  this.drawNativeClient = function() {
    // Drawing will be done into the plugin's display shared memory.
    var callTime = 0;
    var beforeCall = new Date();
    this.naclModule_.set_region(getFloatValue('xleft'),
                                getFloatValue('ytop'),
                                getFloatValue('xright'),
                                getFloatValue('ybottom'));
    this.naclModule_.display();
    var afterCall = new Date();
    var diffTime = afterCall.getTime() - beforeCall.getTime();
    callTime += diffTime;
    return callTime;
  }

  // Moves the palette selector one step, resulting in a color rotation.
  // It also causes a redraw to the windowing system.
  this.shiftColors = function() {
    // Change the color map used to display.
    this.naclModule_.shift_colors(1);
    this.naclModule_.display();
  }

  // Toggles repeated color shifting ("psychedelic mode").
  this.toggleColorShift = function() {
    var cycle_colors_img = document.getElementById('cycle_colors_img');
    if (this.colorShifting_ === null) {
      this.colorShifting_ = setInterval(
                                function() { nav.mandel.shiftColors(); },
                                40);
      cycle_colors_img.src = "images/cycle_colors_in.png";
    } else {
      clearTimeout(this.colorShifting_);
      cycle_colors_img.src ="images/cycle_colors_out.png";
      this.colorShifting_ = null;
    }
  }

  // Compute and display the entire NativeClient mandelbrot picture, including
  // printing the gathered timings.
  this.timedDraw = function() {
    // Compute and display the picture.
    var testTime = this.drawNativeClient(canvasWidth);
    // Update the time element with compute and display manipulation times.
    var timeElement = document.getElementById('nacl_time');
    timeElement.innerHTML = '<strong>FPS: </strong>' + (1000.0 / testTime).toFixed(2) + '<br>';
  }
};

// Before scripting we need to ensure the Native Client module is loaded.
var startupTimeout;

var postLoadInit = function() {
  if (1 == plugin.__moduleReady) {
    clearTimeout(startupTimeout);
    // Set up the display size and shared memory region.
    canvasWidth = parseFloat(document.getElementById('nacl').width);
    plugin.setup(canvasWidth);
    // Create an instance for color/viewport manipulation.
    var mandel = new NaClMandel(plugin);
    // Create an instance for navigation.
    nav = new NavigationClass(mandel);
    // Set the default view values.
    nav.setDefaults();
  } else {
    if (plugin.__moduleReady == undefined) {
      alert('The Native Client plugin was unable to load');
      return;
    }
    startupTimeout = setTimeout(postLoadInit, 100);
  }
}

// The page begins by loading the NativeClient module, mandel_nav.nexe.
function Init() {
 // Remember the NativeClient plugin element.
 plugin = document.getElementById('nacl');
 // Wait for the module to finish initializing before scripting.
 postLoadInit();
}
