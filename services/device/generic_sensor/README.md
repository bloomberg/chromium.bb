# Sensors

`services/device/generic_sensor` contains the platform-specific parts of the Sensor APIs
implementation.

Sensors Mojo interfaces are defined in the `services/public/interfaces` subdirectory.

The JS bindings are implemented in `third_party/WebKit/Source/modules/sensor`.


## Platform Support

Support for the SensorTypes defined by the Mojo interface is summarized in this
table. An empty cell indicates that the sensor type is not supported on that
platform.

| SensorType                        | Android                   | Linux          | macOS                                 | Windows                                   |
| --------------------------------- | ------------------------- | -------------- | ------------------------------------- | ----------------------------------------- |
| AMBIENT_LIGHT                     | TYPE_LIGHT                | in_illuminance | AppleLMUController                    | SENSOR_TYPE_AMBIENT_LIGHT                 |
| PROXIMITY                         |                           |                |                                       |                                           |
| ACCELEROMETER                     | TYPE_ACCELEROMETER        | in_accel       | SMCMotionSensor                       | SENSOR_TYPE_ACCELEROMETER_3D              |
| LINEAR_ACCELEROMETER              | TYPE_LINEAR_ACCELEROMETER |                |                                       | ACCELEROMETER (*)                         |
| GYROSCOPE                         | TYPE_GYROSCOPE            | in_anglvel     |                                       | SENSOR_TYPE_GYROMETER_3D                  |
| MAGNETOMETER                      | TYPE_MAGNETIC_FIELD       | in_magn        |                                       | SENSOR_TYPE_COMPASS_3D                    |
| PRESSURE                          |                           |                |                                       |                                           |
| ABSOLUTE_ORIENTATION_EULER_ANGLES |                           |                |                                       | SENSOR_TYPE_INCLINOMETER_3D               |
| ABSOLUTE_ORIENTATION_QUATERNION   | TYPE_ROTATION_VECTOR      |                |                                       | SENSOR_TYPE_AGGREGATED_DEVICE_ORIENTATION |
| RELATIVE_ORIENTATION_EULER_ANGLES |                           |                | ACCELEROMETER (*)                     |                                           |
| RELATIVE_ORIENTATION_QUATERNION   | TYPE_GAME_ROTATION_VECTOR |                | RELATIVE_ORIENTATION_EULER_ANGLES (*) |                                           |

(Note: "*" means the sensor type is provided by sensor fusion.)

### Android

Sensors are implemented by passing through values provided by the
[Sensor](https://developer.android.com/reference/android/hardware/Sensor.html)
class. The values in the "Android" column of the table above correspond to the
integer constants from the android.hardware.Sensor used to provide data for a
SensorType.

### Linux (and Chrome OS)

Sensors are implemented by reading values from the IIO subsystem. The values in
the "Linux" column of the table above are the prefix of the sysfs files Chrome
searches for to provide data for a SensorType.

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
the sensor type GUIDs used to provide data for a SensorType.

## Testing

Sensors platform unit tests are located in the current directory and its
subdirectories.

The sensors unit tests file for Android is
`android/junit/src/org/chromium/device/sensors/PlatformSensorAndProviderTest.java`.

Sensors browser tests are located in `content/test/data/generic_sensor`.


## Design Documents

Please refer to the [design documentation](https://docs.google.com/document/d/1Ml65ZdW5AgIsZTszk4mD_ohr40pcrdVFOIf0ZtWxDv0)
for more details.
