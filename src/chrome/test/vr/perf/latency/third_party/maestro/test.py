import glob
import re

def GetTtyDevices(tty_pattern, vendor_ids):
  """Finds all devices connected to tty that match a pattern and device id.

  If a serial device is connected to the computer via USB, this function
  will check all tty devices that match tty_pattern, and return the ones
  that have vendor identification number in the list vendor_ids.

  Args:
    tty_pattern: The search pattern, such as r'ttyACM\d+'.
    vendor_ids: The list of 16-bit USB vendor ids, such as [0x2a03].

  Returns:
    A list of strings of tty devices, for example ['ttyACM0'].
  """
  product_string = 'PRODUCT='
  sys_class_dir = '/sys/class/tty/'

  tty_devices = glob.glob(sys_class_dir + '*')

  matcher = re.compile('.*' + tty_pattern)
  tty_matches = [x for x in tty_devices if matcher.search(x)]
  tty_matches = [x[len(sys_class_dir):] for x in tty_matches]

  found_devices = []
  for match in tty_matches:
    class_filename = sys_class_dir + match + '/device/uevent'
    with open(class_filename, 'r') as uevent_file:
      # Look for the desired product id in the uevent text.
      for line in uevent_file:
        if product_string in line:
          ids = line[len(product_string):].split('/')
          ids = [int(x, 16) for x in ids]

          for desired_id in vendor_ids:
            if desired_id in ids:
              found_devices.append(match)

  return found_devices

print GetTtyDevices(r'ttyACM\d+', [0x1ffb])
