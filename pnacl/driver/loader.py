import sys
import driver_tools
import os

# This is called with:
# loader.py <toolname> <args>

pydir = os.path.dirname(os.path.abspath(sys.argv[0]))
bindir = os.path.dirname(pydir)

toolname = sys.argv[1]
extra_args = sys.argv[2:]

module = __import__(toolname)
argv = [os.path.join(bindir, toolname)] + extra_args

driver_tools.SetupSignalHandlers()
ret = driver_tools.DriverMain(module, argv)
driver_tools.DriverExit(ret, is_final_exit=True)
