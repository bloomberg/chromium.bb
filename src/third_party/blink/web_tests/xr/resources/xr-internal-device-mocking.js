'use strict';

/* This file contains extensions to the base mocking from the WebPlatform tests
 * for interal tests. The main mocked objects are found in
 * ../external/wpt/resources/chromium/webxr-test.js. */

MockRuntime.prototype.base_getFrameData = MockRuntime.prototype.getFrameData;

MockRuntime.prototype.getFrameData = function() {
  return this.base_getFrameData().then((result) => {
    if (result.frameData && result.frameData.pose && this.input_sources_) {
      let input_states = [];
      for (let i = 0; i < this.input_sources_.length; ++i) {
        input_states.push(this.input_sources_[i].getInputSourceState());
      }

      result.frameData.pose.inputState = input_states;
    }

    return result;
  });
};

MockRuntime.prototype.addInputSource = function(source) {
  if (!this.input_sources_) {
    this.input_sources_ = [];
    this.next_input_source_index_ = 1;
  }
  let index = this.next_input_source_index_;
  source.source_id_ = index;
  this.next_input_source_index_++;
  this.input_sources_.push(source);
};

MockRuntime.prototype.removeInputSource = function(source) {
  if (!this.input_sources_)
    return;

  for (let i = 0; i < this.input_sources_.length; ++i) {
    if (source.source_id_ == this.input_sources_[i].source_id_) {
      this.input_sources_.splice(i, 1);
      break;
    }
  }
};

MockRuntime.prototype.setHitTestResults = function(results) {
  this.hittest_results_ = results;
};

MockRuntime.prototype.requestHitTest = function(ray) {
  var hit_results = this.hittest_results_;
  if (!hit_results) {
    var hit = new device.mojom.XRHitResult();
    hit.hitMatrix = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
    hit_results = {results: [hit]};
  }
  return Promise.resolve(hit_results);
};

MockRuntime.prototype.setResetPose = function(to) {
  if (this.pose_) {
    this.pose_.poseReset = to;
  }
};

MockRuntime.prototype.setStageTransform = function(value) {
  if (value) {
    if (!this.displayInfo_.stageParameters) {
      this.displayInfo_.stageParameters = {
        standingTransform: value,
        sizeX: 1.5,
        sizeZ: 1.5,
      };
    } else {
      this.displayInfo_.stageParameters.standingTransform = value;
    }
  } else if (this.displayInfo_.stageParameters) {
    this.displayInfo_.stageParameters = null;
  }

  this.sessionClient_.onChanged(this.displayInfo_);
};

MockRuntime.prototype.getSubmitFrameCount = function() {
  return this.presentation_provider_.submit_frame_count_;
};

MockRuntime.prototype.getMissingFrameCount = function() {
  return this.presentation_provider_.missing_frame_count_;
};

class MockXRInputSource {
  constructor() {
    this.source_id_ = 0;
    this.primary_input_pressed_ = false;
    this.primary_input_clicked_ = false;
    this.grip_ = null;

    this.target_ray_mode_ = 'gaze';
    this.pointer_offset_ = null;
    this.emulated_position_ = false;
    this.handedness_ = '';
    this.desc_dirty_ = true;
  }

  get primaryInputPressed() {
    return this.primary_input_pressed_;
  }

  set primaryInputPressed(value) {
    if (this.primary_input_pressed_ && !value) {
      this.primary_input_clicked_ = true;
    }
    this.primary_input_pressed_ = value;
  }

  get grip() {
    if (this.grip_) {
      return this.grip_.matrix;
    }
    return null;
  }

  set grip(value) {
    if (!value) {
      this.grip_ = null;
      return;
    }
    this.grip_ = new gfx.mojom.Transform();
    this.grip_.matrix = new Float32Array(value);
  }

  get targetRayMode() {
    return this.target_ray_mode_;
  }

  set targetRayMode(value) {
    if (this.target_ray_mode_ != value) {
      this.desc_dirty_ = true;
      this.target_ray_mode_ = value;
    }
  }

  get pointerOffset() {
    if (this.pointer_offset_) {
      return this.pointer_offset_.matrix;
    }
    return null;
  }

  set pointerOffset(value) {
    this.desc_dirty_ = true;
    if (!value) {
      this.pointer_offset_ = null;
      return;
    }
    this.pointer_offset_ = new gfx.mojom.Transform();
    this.pointer_offset_.matrix = new Float32Array(value);
  }

  get emulatedPosition() {
    return this.emulated_position_;
  }

  set emulatedPosition(value) {
    if (this.emulated_position_ != value) {
      this.desc_dirty_ = true;
      this.emulated_position_ = value;
    }
  }

  get handedness() {
    return this.handedness_;
  }

  set handedness(value) {
    if (this.handedness_ != value) {
      this.desc_dirty_ = true;
      this.handedness_ = value;
    }
  }

  getInputSourceState() {
    let input_state = new device.mojom.XRInputSourceState();

    input_state.sourceId = this.source_id_;

    input_state.primaryInputPressed = this.primary_input_pressed_;
    input_state.primaryInputClicked = this.primary_input_clicked_;

    input_state.grip = this.grip_;

    if (this.desc_dirty_) {
      let input_desc = new device.mojom.XRInputSourceDescription();

      input_desc.emulatedPosition = this.emulated_position_;

      switch (this.target_ray_mode_) {
        case 'gaze':
          input_desc.targetRayMode = device.mojom.XRTargetRayMode.GAZING;
          break;
        case 'tracked-pointer':
          input_desc.targetRayMode = device.mojom.XRTargetRayMode.POINTING;
          break;
      }

      switch (this.handedness_) {
        case 'left':
          input_desc.handedness = device.mojom.XRHandedness.LEFT;
          break;
        case 'right':
          input_desc.handedness = device.mojom.XRHandedness.RIGHT;
          break;
        default:
          input_desc.handedness = device.mojom.XRHandedness.NONE;
          break;
      }

      input_desc.pointerOffset = this.pointer_offset_;

      input_state.description = input_desc;

      this.desc_dirty_ = false;
    }

    return input_state;
  }
}
