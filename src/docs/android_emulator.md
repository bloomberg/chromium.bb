# Using an Android Emulator
Always use x86 emulators. Although arm emulators exist, they are so slow that
they are not worth your time.

## Building for Emulation
You need to target the correct architecture via GN args:
```gn
target_cpu = "x86"
```

## Creating an Emulator Image
By far the easiest way to set up emulator images is to use Android Studio.
If you don't have an [Android Studio project](android_studio.md) already, you
can create a blank one to be able to reach the Virtual Device Manager screen.

Refer to: https://developer.android.com/studio/run/managing-avds.html

Where files live:
 * System partition images are stored within the sdk directory.
 * Emulator configs and data partition images are stored within
   `~/.android/avd/`.

### Choosing a Skin
Choose a skin with a small screen for better performance (unless you care about
testing large screens).

### Choosing an Image
Android Studio's image labels roughly translate to the following:

| AVD "Target" | GMS? | Build Properties |
| --- | --- | --- |
| Google Play | This has GMS | `user`/`release-keys` |
| Google APIs | This has GMS | `userdebug`/`dev-keys` |
| No label | AOSP image, does not have GMS | `eng`/`test-keys` |

*** promo
If you're not sure which to use, **choose Google APIs**.
***

### Configuration
"Show Advanced Settings" > scroll down:
* Set internal storage to 4000MB (component builds are really big).
* Set SD card to 1000MB (our tests push a lot of files to /sdcard).

### Known Issues
 * Our test & installer scripts do not work with pre-MR1 Jelly Bean.
 * Component builds do not work on pre-KitKat (due to the OS having a max
   number of shared libraries).
 * Jelly Bean and KitKat images sometimes forget to mount /sdcard :(.
   * This causes tests to fail.
   * To ensure it's there: `adb -s emulator-5554 shell mount` (look for /sdcard)
   * Can often be fixed by editing `~/.android/avd/YOUR_DEVICE/config.ini`.
     * Look for `hw.sdCard=no` and set it to `yes`

### Cloning an Image
Running tests on two emulators is twice as fast as running on one. Rather
than use the UI to create additional avds, you can clone an existing one via:

```shell
$ tools/android/emulator/clone_avd.py \
    --source-ini ~/.android/avd/EMULATOR_ID.ini \
    --dest-ini ~/.android/avd/EMULATOR_ID_CLONED.ini \
    --display-name "Cloned Emulator"
```

## Starting an Emulator from the Command Line
Refer to: https://developer.android.com/studio/run/emulator-commandline.html.

*** promo
Ctrl-C will gracefully close an emulator.
***

### Basic Command Line Use
```shell
$ ~/Android/Sdk/emulator/emulator @EMULATOR_ID
```

### Running a Headless Emulator
You can run an emulator without creating a window on your desktop (useful for
`ssh`):
```shell
$ sudo apt-get install xvfb-run
$ xvfb-run ~/Android/Sdk/emulator/emulator -gpu off @EMULATOR_ID
```

### Writable system partition
Unlike physical devices, an emulator's `/system` partition cannot be modified by
default (even on rooted devices). If you need to do so (such as to remove a
system app), you can start your emulator like so:
```shell
$ ~/Android/Sdk/emulator/emulator -writable-system @EMULATOR_ID
```

### Remote Desktop
For better graphics performance, use virtualgl (Googlers, see
http://go/virtualgl):
```shell
$ vglrun ~/Android/Sdk/emulator/emulator @EMULATOR_ID
```

## Using an Emulator
 * Emulators show up just like devices via `adb devices`
   * Device serials will look like "emulator-5554", "emulator-5556", etc.

