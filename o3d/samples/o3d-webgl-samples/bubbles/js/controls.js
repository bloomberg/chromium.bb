/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Creates a jQuery UI Slider and stores associated options and callbacks.
 * @param {Object} options Options for the slider. See jQuery documentation.
 * @param {function} cb Callback to run when value is changed.
 */
var SliderSet = function(options, cb) {
  this.parent = $('<div class="slider-set"></div>');

  // Span for the title of this option.
  this.parent.append('<span>' + options.name + '</span>');

  // Span for the value of this option.
  this.valueWrite = $('<span class="value"></span>');
  this.parent.append(this.valueWrite);

  // Make the slider.
  var slider = $('<div></div>');
  var that = this;
  options.slide = function(ev, ui) {
    that.updateValue(ui.values ? ui.values : ui.value);
  };
  options.stop = function(ev, ui) {
    cb(ui.values ? ui.values : ui.value);
  }
  slider.slider(options);

  // Wrap slider in div and append to parent.
  var sliderWrapper = $('<div class="slider-wrap" />');
  sliderWrapper.append(slider);
  this.parent.append(sliderWrapper);
}

/**
 * The parent div of this particular option set.
 * @type {DOMElement}
 */
SliderSet.prototype.parent = null;

/**
 * Container the displays the current value of the slider.
 * @type {DOMElement}
 */
SliderSet.prototype.valueWrite = null;


/**
 * Updates the value being shown to the user.
 * @param {Number|!Array.<number>} values The value(s) of the slider.
 */
SliderSet.prototype.updateValue = function(values) {
  var print = '';
  if (values.length) { // is Array
    print = 'Range: ' + values[0] + ' to ' + values[1];
  } else {
    print = values + '';
  }
  this.valueWrite.html(print);
}


/**
 * Creates and appends the controls.
 */
function buildControls() {
  var bubblePanel = $("#opt-bubble");
  var iridescencePanel = $("#tex-iridescence");
  var noisePanel = $("#tex-perlin");
  var modPanel = $("#tex-modulation")
  var g = bsh.Globals;

  var sliders = [
      { name: 'Number of Bubbles',
        min: 1,
        max: 15,
        value: g.kBubbleCount,
        step: 1,
        range: false,
        parent: bubblePanel,
        cb: function(values) {
          bsh.Globals.kBubbleCount = values;
          g_demo.regenerateBubbles();
        }
      },
      { name: 'Bubble Size Range',
        min: 0.1,
        max: 2.5,
        step: 0.1,
        values: g.kBubbleScale,
        range: true,
        parent: bubblePanel,
        cb: function(values) {
          bsh.Globals.kBubbleScale = values;
          g_demo.regenerateBubbles();
        }
      },
      { name: 'Bubble Thickness Range',
        min: 0.1,
        max: 1.0,
        step: 0.1,
        values: g.kBubbleThickness,
        range: true,
        parent: bubblePanel,
        cb: function(values) {
          bsh.Globals.kBubbleThickness = values;
          g_demo.regenerateBubbles();
        }
      },
      { name: 'Bubble Movement',
          min: 0,
          max: 0.2,
          step: 0.01,
          value: g.kBubbleSpin,
          range: false,
          parent: bubblePanel,
          cb: function(values) {
            bsh.Globals.kBubbleSpin = values;
            g_demo.regenerateBubbles();
          }
      },
      { name: 'Pattern Distortion',
        min: 0,
        max: 1.5,
        step: 0.1,
        values: g.kNoiseRatio,
        range: true,
        parent: bubblePanel,
        cb: function(values) {
          bsh.Globals.kNoiseRatio = values;
          g_demo.regenerateBubbles();
        }
      },
      { name: 'Iridescence Frequency',
        min: 500,
        max: 1000,
        step: 20,
        value: g.kMaxWavelength,
        range: false,
        parent: iridescencePanel,
        cb: function(values) {
          bsh.Globals.kMaxWavelength = values;
          g_demo.regenerateIridescence();
        }
      },
      { name: 'Iridescence Refraction Index',
        min: 1,
        max: 2,
        step: 0.01,
        value: g.kRefractionIndex,
        range: false,
        parent: iridescencePanel,
        cb: function(values) {
          bsh.Globals.kRefractionIndex = values;
          g_demo.regenerateIridescence();
        }
      },
      { name: 'Noise Frequency',
        min: 4,
        max: 12,
        step: 1,
        value: g.kFrequency,
        range: false,
        parent: noisePanel,
        cb: function(values) {
          bsh.Globals.kFrequency = values;
          g_demo.regenerateNoise();
        }
      },
      { name: 'Blip Radius Range',
        min: 50,
        max: 400,
        step: 25,
        values: g.kBlipSize,
        range: true,
        parent: modPanel,
        cb: function(values) {
          bsh.Globals.kBlipSize = values;
          g_demo.regenerateModulation();
        }
      },
      { name: 'Number of Blips',
        min: 0,
        max: g.kMaxBlips,
        step: 1,
        value: g.kNumBlips,
        range: false,
        parent: modPanel,
        cb: function(values) {
          bsh.Globals.kNumBlips = values;
          g_demo.regenerateModulation();
        }
      },
      { name: 'Distortion Factor',
        min: 0,
        max: 1,
        step: 0.05,
        value: g.kBlipDistortion,
        range: false,
        parent: modPanel,
        cb: function(values) {
          g_blipDistortion.value = values;
        }
      }
  ];

  // Append textures to their divs.
  $("#tex-iridescence").prepend(g_demo.iridescence_source.canvas);
  $("#tex-perlin").prepend(g_demo.noise_source.canvas);
  $("#tex-modulation").prepend(g_demo.mod_source.canvas);

  // Make the sliders.
  for (var i = 0; i < sliders.length; i++) {
    var specs = sliders[i];
    var s = new SliderSet(specs, specs.cb);
    specs.parent.prepend(s.parent);
    s.updateValue(specs.value ? specs.value : specs.values);
  }

  // Add event listeners to specific links.
  $("#toggle-env").click(function() {
    g_useCubeMap.value = !g_useCubeMap.value;
    if (g_useCubeMap.value) {
      $(this).html("On. Turn off?");
    } else {
      $(this).html("Off. Turn on?");
    }
    return false;
  });

  $(".closer").each(function(i) {
  var index = i;
    $(this).click(function() {
      $(this).parent().slideUp();
      var checkboxes = $("input[type=checkbox]");
      $(checkboxes[index]).attr("checked", false);
      return false;
    });
  });

  // 'q' closes all the open panels.
  // 1/2 toggle whether the color effect is doubled for more visible bubbles.
  window.document.onkeypress = function(e) {
    var event = e || window.event;
    if (event.metaKey)
      return;
    var keyChar = String.fromCharCode(o3djs.event.getEventKeyChar(event));
    keyChar = keyChar.toLowerCase();
    if (keyChar == 'q') {
      hideAll();
    } else if (keyChar == '1') {
      g_demo.renderThickerBubbles(false);
    } else if (keyChar == '2'){
      g_demo.renderThickerBubbles(true);
    }
  };
}

/**
 * Hides any open option panels.
 */
function hideAll() {
  $(".closer").click();
}

/**
 * Toggles which option panel is being shown.
 * @param {number} type Index of the associated option panel in #controls.
 * @param {DOMElement} checkbox The checkbox element.
 */
function toggle(type, checkbox) {
  var options = $("#controls").children();

  if (! $(checkbox).attr('checked')) {
    $(options[type]).slideUp();
  } else {
    var target = $(options[type]);
    if (target.css("display") == "block") {
      return; // Already showing.
    }
    $("#controls").children().slideUp();
    target.slideDown();
    $("#floatControls input").each(function(i) {
      if (i != type) {
        $(this).attr("checked", false);
      }
    });
  }
}

/**
 * Changes the background color of the o3d container based on the current values
 * of the input fields bg-red, bg-green and bg-blue.
 */
function updateClearColor() {
  function to255(a) {
    a = parseInt(a);
    if (isNaN(a)) {
      a = 0;
    }
    return Math.max(0, Math.min(255, a));
  }
  var red = to255($("#bg-red").val());
  var green = to255($("#bg-green").val());
  var blue = to255($("#bg-blue").val());
  $("#bg-red").val(red);
  $("#bg-green").val(green);
  $("#bg-blue").val(blue);

  // Convert back to [0, 1] for o3d.
  g_viewInfo.clearBuffer.clearColor = [red / 255, green / 255, blue / 255, 1.0];
}
