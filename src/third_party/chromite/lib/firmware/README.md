# AP Firmware sample usage guide:

## Building
To build the AP Firmware for foo:
  cros build-ap -b foo

To build the AP Firmware only for foo-variant:
  cros build-ap -b foo --fw-name foo-variant

## Flashing
Requires servod process to be running if flashing via servo

To flash your zork DUT with an IP of 1.1.1.1 via SSH:
  cros flash-ap -b zork -i /path/to/image.bin -d ssh://1.1.1.1

To flash your volteer DUT via SERVO on the default port (9999):
  cros flash-ap -d servo:port -b volteer -i /path/to/image.bin

To flash your volteer DUT via SERVO on port 1234:
  cros flash-ap -d servo:port:1234 -b volteer -i /path/to/image.bin

## Add support for new board
Create ${BOARD}.py in chromite/lib/firmware/ap_firmware_config.

 Define the following variables:

    BUILD_WORKON_PACKAGES as a list of all packages that should be cros_workon'd
      before building.

    BUILD_PACKAGES as a list of all packages that should be emerged during the
      build process.

  Define the following functions:

    is_fast_required: #TODO saklien@ is this function required for defining new boards?
      Returns true if --fast is necessary to flash successfully.
      The configurations in this function consistently fail on the verify step,
      adding --fast removes verification of the flash and allows these configs to
      flash properly. Meant to be a temporary hack until b/143240576 is fixed.
      Args:
        _use_futility (bool): True if futility is to be used, False if
          flashrom.
        _servo (str): The type name of the servo device being used.
      Returns:
        bool: True if fast is necessary, False otherwise.

    get_commands:
      Get specific flash commands for this board
      Each board needs specific commands including the voltage for Vref, to turn
      on and turn off the SPI flash. These commands can be found in the care and
      feeding doc for your board, any command that needs to be run before
      flashing should be included in dut_control_on and anything run after flashing
      should be in dut_control_off.

      Args:
        servo (servo_lib.Servo): The servo connected to the target DUT.

      Returns:
        list: [dut_control_on, dut_control_off, flashrom_cmd, futility_cmd]
          dut_control*=2d arrays formmated like [["cmd1", "arg1", "arg2"],
                                                 ["cmd2", "arg3", "arg4"]]
          where cmd1 will be run before cmd2
          flashrom_cmd=command to flash via flashrom
          futility_cmd=command to flash via futility

See dedede.py for examples of each function/variable.
