#!/usr/bin/env python3

import sys, os, subprocess, tempfile, shutil


def cmd (command):
	# https://stackoverflow.com/a/4408409 as we might have huge output sometimes
	with tempfile.TemporaryFile () as tempf:
		p = subprocess.Popen (command, stderr=tempf)

		try:
			p.wait (timeout=int (os.environ.get ("HB_TEST_SUBSET_FUZZER_TIMEOUT", "12")))
			tempf.seek (0)
			text = tempf.read ()

			#TODO: Detect debug mode with a better way
			is_debug_mode = b"SANITIZE" in text

			return ("" if is_debug_mode else text.decode ("utf-8").strip ()), p.returncode
		except subprocess.TimeoutExpired:
			return 'error: timeout, ' + ' '.join (command), 1


srcdir = os.environ.get ("srcdir", ".")
EXEEXT = os.environ.get ("EXEEXT", "")
top_builddir = os.environ.get ("top_builddir", ".")
hb_subset_fuzzer = os.path.join (top_builddir, "hb-subset-fuzzer" + EXEEXT)

if not os.path.exists (hb_subset_fuzzer):
        if len (sys.argv) < 2 or not os.path.exists (sys.argv[1]):
                print ("""Failed to find hb-subset-fuzzer binary automatically,
please provide it as the first argument to the tool""")
                sys.exit (1)

        hb_subset_fuzzer = sys.argv[1]

print ('hb_subset_fuzzer:', hb_subset_fuzzer)
fails = 0

libtool = os.environ.get('LIBTOOL')
valgrind = None
if os.environ.get('RUN_VALGRIND', ''):
	valgrind = shutil.which ('valgrind')
	if valgrind is None:
		print ("""Valgrind requested but not found.""")
		sys.exit (1)
	if libtool is None:
		print ("""Valgrind support is currently autotools only and needs libtool but not found.""")


def run_dir (parent_path):
	global fails
	for file in os.listdir (parent_path):
		path = os.path.join(parent_path, file)
		# TODO: Run on all the fonts not just subset related ones
		if "subset" not in path: continue

		print ("running subset fuzzer against %s" % path)
		if valgrind:
			text, returncode = cmd (libtool.split(' ') + ['--mode=execute', valgrind + ' --leak-check=full --show-leak-kinds=all --error-exitcode=1', '--', hb_subset_fuzzer, path])
		else:
			text, returncode = cmd ([hb_subset_fuzzer, path])
			if 'error' in text:
				returncode = 1

		if (not valgrind or returncode) and text.strip ():
			print (text)

		if returncode != 0:
			print ("failed for %s" % path)
			fails = fails + 1


run_dir (os.path.join (srcdir, "..", "subset", "data", "fonts"))
run_dir (os.path.join (srcdir, "fonts"))

if fails:
        print ("%i subset fuzzer related tests failed." % fails)
        sys.exit (1)
