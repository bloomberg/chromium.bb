# Chromium web_runner
This directory contains the web_runner implementation. Web_runner enables
Fuchsia applications to embed Chrome frames for rendering web content.


### Building and deploying web_runner
When you build web_runner, Chromium will automatically generate scripts for
you that will automatically provision a device with Fuchsia and then install
`web_runner` and its dependencies.

To build and run web_runner, follow these steps:

0. Ensure that you have a device ready to boot into Fuchsia.

   If you wish to have WebRunner manage the OS deployment process, then you
   should have the device booting into
   [Zedboot](https://fuchsia.googlesource.com/zircon/+/master/docs/targets/bootloader_setup.md).

1. Build web_runner.

   ```
   $ autoninja -C out/Debug webrunner
   ```

2. Install web_runner.

    * **For devices running Zedboot**

          ```
          $ out/Debug/bin/install_webrunner -d
          ```

    * **For devices already running Fuchsia**

          You will need to add command line flags specifying the device's IP
          address and the path to the `ssh_config` used by the device
          (located at `FUCHSIA_OUT_DIR/ssh-keys/ssh_config`):

          ```
          $ out/Debug/bin/install_webrunner -d --ssh-config PATH_TO_SSH_CONFIG
          --host DEVICE_IP
          ```

3. Run "tiles" on the device.

   ```
   $ run tiles&
   ```

4. Press the OS key on your device to switch back to terminal mode.
   (Also known as the "Windows key" or "Super key" on many keyboards).

5. Launch a webpage.

   ```
   $ tiles_ctl add https://www.chromium.org/
   ```

6. Press the OS key to switch back to graphical view. The browser window should
   be displayed and ready to use.

7. You can deploy and run new versions of Chromium without needing to reboot.

   First kill any running processes:

      ```
      $ killall chromium; killall web_runner
      ```

   Then repeat steps 1 through 6 from the installation instructions, excluding
   step #3 (running Tiles).


### Closing a webpage

1. Press the Windows key to return to the terminal.

2. Instruct tiles_ctl to remove the webpage's window tile. The tile's number is
   reported by step 6, or it can be found by running `tiles_ctl list` and
   noting the ID of the "url" entry.

   ```shell
   $ tiles_ctl remove TILE_NUMBER
   ```

