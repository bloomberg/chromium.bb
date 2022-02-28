// RUN: tf-opt %s -split-input-file -verify-diagnostics -tf-device-host-launch-to-outside-compiled | FileCheck %s

// Tests invalid device error returned when invalid device set on module.

// expected-error@+1 {{not a valid device}}
module attributes {tf.versions = {producer = 888 : i32}, tf.devices = ["bad_device"]} {
  func @bad_device_error() -> () {
    "tf_device.cluster"() ( {
      "tf.A"() : () -> ()
      "tf_device.launch"() ( {
        "tf.B"() : () -> ()
	tf_device.return
      }) {device = "/job:worker/replica:0/task:0/device:CPU:0"} : () -> ()
      "tf.C"() : () -> ()
      tf_device.return
    }) {num_cores_per_replica = 1, topology = "", device_assignment = []} : () -> ()
    return
  }
}

// -----

// Checks that model parallelism doesn't cause error.

module attributes {tf.versions = {producer = 888 : i32}, tf.devices = ["/job:worker/replica:0/task:0/device:CPU:0", "/job:worker/replica:0/task:0/device:TPU_SYSTEM:0", "/job:worker/replica:0/task:0/device:TPU:0"]} {
  // CHECK-LABEL: func @model_parallelism
  func @model_parallelism() -> () {
    // CHECK:      "tf_device.launch"
    "tf_device.cluster"() ( {
      "tf.A"() : () -> ()
      "tf_device.launch"() ( {
        "tf.B"() : () -> ()
	tf_device.return
      }) {device = "/job:worker/replica:0/task:0/device:CPU:0"} : () -> ()
      "tf.C"() : () -> ()
      tf_device.return
    }) {num_cores_per_replica = 2, topology = "", device_assignment = []} : () -> ()
    return
  }
}

// -----

module attributes {tf.versions = {producer = 888 : i32}, tf.devices = ["/job:worker/replica:0/task:0/device:CPU:0", "/job:worker/replica:0/task:0/device:TPU_SYSTEM:0", "/job:worker/replica:0/task:0/device:TPU:0"]} {

  // Tests the unwrap of unreplicated launch of a single outside compiled op with no input or output dependencies.

  // CHECK-LABEL: func @single_op_launch_not_host
  func @single_op_launch_not_host() -> () {
    // CHECK:      "tf.A"
    // CHECK:      "tf_device.launch"
    // CHECK:        "tf.B"
    // CHECK-NOT:    _xla_outside_compilation
    // CHECK:      device = "/job:worker/replica:0/task:0/device:TPU:0"
    // CHECK:      "tf.C"
    // CHECK-NEXT: tf_device.return
    "tf_device.cluster"() ( {
      "tf.A"() : () -> ()
      "tf_device.launch"() ( {
        "tf.B"() : () -> ()
	tf_device.return
      }) {device = "/job:worker/replica:0/task:0/device:TPU:0"} : () -> ()
      "tf.C"() : () -> ()
      tf_device.return
    }) {num_cores_per_replica = 1, topology = "", device_assignment = []} : () -> ()
    return
  }

  // CHECK-LABEL: func @single_op_hostlaunch_no_input_output
  func @single_op_hostlaunch_no_input_output() -> () {
    // CHECK:      "tf.A"
    // CHECK-NOT:  "tf_device.launch"
    // CHECK-NEXT: "tf.B"
    // CHECK-SAME:    _xla_outside_compilation
    // CHECK:      "tf.C"
    // CHECK-NEXT: tf_device.return
    "tf_device.cluster"() ( {
      "tf.A"() : () -> ()
      "tf_device.launch"() ( {
        "tf.B"() : () -> ()
	tf_device.return
      }) {device = "/job:worker/replica:0/task:0/device:CPU:0"} : () -> ()
      "tf.C"() : () -> ()
      tf_device.return
    }) {num_cores_per_replica = 1, topology = "", device_assignment = []} : () -> ()
    return
  }

  // CHECK-LABEL: func @single_op_host_launch_input_output
  func @single_op_host_launch_input_output() -> () {
    // CHECK:      %[[A_OUTPUT:[0-9]*]] = "tf.A"
    // CHECK-NOT:  "tf_device.launch"
    // CHECK-NEXT: %[[B_OUTPUT:[0-9]*]] = "tf.B"(%[[A_OUTPUT]])
    // CHECK-SAME:    _xla_outside_compilation
    // CHECK:      "tf.C"(%[[B_OUTPUT]])
    // CHECK-NEXT: tf_device.return
    "tf_device.cluster"() ( {
      %1 = "tf.A"() : () -> (tensor<?xi32>)
      %2 = "tf_device.launch"() ( {
        %3 = "tf.B"(%1) : (tensor<?xi32>) -> (tensor<?xi32>)
	tf_device.return %3 : tensor<?xi32>
      }) {device = "/job:worker/replica:0/task:0/device:CPU:0"} : () -> (tensor<?xi32>)
      %4 = "tf.C"(%2) : (tensor<?xi32>) -> tensor<?xi32>
      tf_device.return
    }) {num_cores_per_replica = 1, topology = "", device_assignment = []} : () -> ()
    return
  }

  // CHECK-LABEL: func @multiple_ops_host_launch_input_output
  func @multiple_ops_host_launch_input_output() -> () {
    // CHECK:      %[[A_OUTPUT:[0-9]*]] = "tf.A"
    // CHECK-NOT:  "tf_device.launch"
    // CHECK-NEXT: %[[B_OUTPUT:[0-9]*]] = "tf.B"(%[[A_OUTPUT]])
    // CHECK-SAME:    _xla_outside_compilation
    // CHECK-NEXT: %[[D_OUTPUT:[0-9]*]] = "tf.D"(%[[B_OUTPUT]])
    // CHECK-SAME:    _xla_outside_compilation
    // CHECK:      "tf.C"(%[[D_OUTPUT]])
    // CHECK-NEXT: tf_device.return
    "tf_device.cluster"() ( {
      %1 = "tf.A"() : () -> (tensor<?xi32>)
      %2 = "tf_device.launch"() ( {
        %3 = "tf.B"(%1) : (tensor<?xi32>) -> (tensor<?xi32>)
        %4 = "tf.D"(%3) : (tensor<?xi32>) -> (tensor<?xi32>)
	tf_device.return %4 : tensor<?xi32>
      }) {device = "/job:worker/replica:0/task:0/device:CPU:0"} : () -> (tensor<?xi32>)
      %5 = "tf.C"(%2) : (tensor<?xi32>) -> tensor<?xi32>
      tf_device.return
    }) {num_cores_per_replica = 1, topology = "", device_assignment = []} : () -> ()
    return
  }

}
