# Sensors

`services/device/generic_sensor` contains the platform-specific parts of the Sensor APIs
implementation.

Sensors Mojo interfaces are defined in the `services/device/public/interfaces` subdirectory.

## Web-exposed Interfaces

### Generic Sensors

The Generic Sensors API is implemented in `third_party/WebKit/Source/modules/sensor` and exposes the following sensor types as JavaScript objects:

* [AbsoluteOrientationSensor] &rarr; ABSOLUTE_ORIENTATION_QUATERNION
* [Accelerometer] &rarr; ACCELEROMETER
* [AmbientLightSensor] &rarr; AMBIENT_LIGHT
* [Gyroscope] &rarr; GYROSCOPE
* [LinearAccelerationSensor] &rarr; LINEAR_ACCELEROMETER
* [Magnetometer] &rarr; MAGNETOMETER
* [RelativeOrientationSensor] &rarr; RELATIVE_ORIENTATION_QUATERNION

[AbsoluteOrientationSensor]: ../../../third_party/WebKit/Source/modules/sensor/AbsoluteOrientationSensor.idl
[Accelerometer]: ../../../third_party/WebKit/Source/modules/sensor/Accelerometer.idl
[AmbientLightSensor]: ../../../third_party/WebKit/Source/modules/sensor/AmbientLightSensor.idl
[Gyroscope]: ../../../third_party/WebKit/Source/modules/sensor/Gyroscope.idl
[LinearAccelerationSensor]: ../../../third_party/WebKit/Source/modules/sensor/LinearAccelerationSensor.idl
[Magnetometer]: ../../../third_party/WebKit/Source/modules/sensor/Magnetometer.idl
[RelativeOrientationSensor]: ../../../third_party/WebKit/Source/modules/sensor/RelativeOrientationSensor.idl

### DeviceOrientation Events

The DeviceOrientation Events API is implemented in `third_party/WebKit/Source/modules/device_orientation` and exposes two events based on the following sensors:

* [DeviceMotionEvent]
  * ACCELEROMETER: populates the `accelerationIncludingGravity` field
  * LINEAR_ACCELEROMETER: populates the `acceleration` field
  * GYROSCOPE: populates the `rotationRate` field
* [DeviceOrientationEvent]
  * ABSOLUTE_ORIENTATION_EULER_ANGLES (when a listener for the `'deviceorientationabsolute'` event is added)
  * RELATIVE_ORIENTATION_EULER_ANGLES (when a listener for the `'deviceorientation'` event is added)

[DeviceMotionEvent]: ../../../third_party/WebKit/Source/modules/device_orientation/DeviceMotionEvent.idl
[DeviceOrientationEvent]: ../../../third_party/WebKit/Source/modules/device_orientation/DeviceOrientationEvent.idl


## Permissions

The device service provides no support for permission checks. When the render process requests access to a sensor type this request is proxied through the browser process by [SensorProviderProxyImpl] which is responsible for checking the permissions granted to the requesting origin.

[SensorProviderProxyImpl]: ../../../content/browser/generic_sensor/sensor_provider_proxy_impl.h

## Platform Support

Support for the SensorTypes defined by the Mojo interface is summarized in this
table. An empty cell indicates that the sensor type is not supported on that
platform.

| SensorType                        | Android                   | Linux                                 | macOS                                 | Windows                                   |
| --------------------------------- | ------------------------- | ------------------------------------- | ------------------------------------- | ----------------------------------------- |
| AMBIENT_LIGHT                     | TYPE_LIGHT                | in_illuminance                        | AppleLMUController                    | SENSOR_TYPE_AMBIENT_LIGHT                 |
| PROXIMITY                         |                           |                                       |                                       |                                           |
| ACCELEROMETER                     | TYPE_ACCELEROMETER        | in_accel                              | SMCMotionSensor                       | SENSOR_TYPE_ACCELEROMETER_3D              |
| LINEAR_ACCELEROMETER              | TYPE_LINEAR_ACCELEROMETER | ACCELEROMETER (*)                     |                                       | ACCELEROMETER (*)                         |
| GYROSCOPE                         | TYPE_GYROSCOPE            | in_anglvel                            |                                       | SENSOR_TYPE_GYROMETER_3D                  |
| MAGNETOMETER                      | TYPE_MAGNETIC_FIELD       | in_magn                               |                                       | SENSOR_TYPE_COMPASS_3D                    |
| PRESSURE                          |                           |                                       |                                       |                                           |
| ABSOLUTE_ORIENTATION_EULER_ANGLES | See below                 |                                       |                                       | SENSOR_TYPE_INCLINOMETER_3D               |
| ABSOLUTE_ORIENTATION_QUATERNION   | See below                 |                                       |                                       | SENSOR_TYPE_AGGREGATED_DEVICE_ORIENTATION |
| RELATIVE_ORIENTATION_EULER_ANGLES | See below                 | ACCELEROMETER (*)                     | ACCELEROMETER (*)                     |                                           |
| RELATIVE_ORIENTATION_QUATERNION   | TYPE_GAME_ROTATION_VECTOR | RELATIVE_ORIENTATION_EULER_ANGLES (*) | RELATIVE_ORIENTATION_EULER_ANGLES (*) |                                           |

(Note: "*" means the sensor type is provided by sensor fusion.)

### Android

Sensors are implemented by passing through values provided by the
[Sensor](https://developer.android.com/reference/android/hardware/Sensor.html)
class. The TYPE_* values in the below descriptions correspond to the integer
constants from the android.hardware.Sensor used to provide data for a
SensorType.

For ABSOLUTE_ORIENTATION_EULER_ANGLES, the following sensor fallback is used:
1. ABSOLUTE_ORIENTATION_QUATERNION (if it uses TYPE_ROTATION_VECTOR
     directly)
2. Combination of ACCELEROMETER and MAGNETOMETER

For ABSOLUTE_ORIENTATION_QUATERNION, the following sensor fallback is used:
1. Use TYPE_ROTATION_VECTOR directly
2. ABSOLUTE_ORIENTATION_EULER_ANGLES

For RELATIVE_ORIENTATION_EULER_ANGLES, converts the data produced by
RELATIVE_ORIENTATION_QUATERNION to euler angles.

### Linux (and Chrome OS)

Sensors are implemented by reading values from the IIO subsystem. The values in
the "Linux" column of the table above are the prefix of the sysfs files Chrome
searches for to provide data for a SensorType. The
RELATIVE_ORIENTATION_EULER_ANGLES sensor type is provided by interpreting the
value that can be read from the ACCELEROMETER. The
RELATIVE_ORIENTATION_QUATERNION sensor type is provided by interpreting the
value that can be read from the RELATIVE_ORIENTATION_EULER_ANGLES.
LINEAR_ACCELEROMETER sensor type is provided by implementing a low-pass-filter
over the values returned by the ACCELEROMETER in order to remove the
contribution of the gravitational force.

### macOS

On this platform there is limited support for sensors. The AMBIENT_LIGHT sensor
type is provided by interpreting the value that can be read from the LMU. The
ACCELEROMETER sensor type is provided by interpreting the value that can be read
from the SMCMotionSensor. The RELATIVE_ORIENTATION_EULER_ANGLES sensor type is
provided by interpreting the value that can be read from the ACCELEROMETER. The
RELATIVE_ORIENTATION_QUATERNION sensor type is provided by interpreting the
value that can be read from the RELATIVE_ORIENTATION_EULER_ANGLES.

### Windows

Sensors are implemented by passing through values provided by the
[Sensor API](https://msdn.microsoft.com/en-us/library/windows/desktop/dd318953(v=vs.85).aspx).
The values in the "Windows" column of the table above correspond to the names of
the sensor type GUIDs used to provide data for a SensorType. The
LINEAR_ACCELEROMETER sensor type is provided by implementing a low-pass-filter
over the values returned by the ACCELEROMETER in order to remove the
contribution of the gravitational force.

## Testing

Sensors platform unit tests are located in the current directory and its
subdirectories.

The sensors unit tests file for Android is
`android/junit/src/org/chromium/device/sensors/PlatformSensorAndProviderTest.java`.

Sensors browser tests are located in `content/test/data/generic_sensor`.


## Design Documents

Please refer to the [design documentation](https://docs.google.com/document/d/1Ml65ZdW5AgIsZTszk4mD_ohr40pcrdVFOIf0ZtWxDv0)
for more details.
