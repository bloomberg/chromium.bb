#! /bin/bash
# script to create a Linux Kernel tree from the DRM tree for diffing etc..
#
# Original author - Dave Airlie (C) 2004 - airlied@linux.ie
#

if [ $# -lt 2 ] ;then
	echo usage: $0 output_dir [2.4\|2.6]
	exit 1
fi

if [ ! -d shared -o ! -d linux ]  ;then
	echo not in DRM toplevel
	exit 1
fi

OUTDIR=$1/drivers/char/drm/
VERS=$2

echo "Copying kernel independent files"
mkdir -p $OUTDIR

( cd linux/ ; make drm_pciids.h )
cp shared-core/*.[ch] $OUTDIR
cp linux-core/*.[ch] $OUTDIR
cp linux-core/Makefile.kernel $OUTDIR/Makefile

if [ $VERS = 2.4 ] ;then
	echo "Copying 2.4 Kernel files"
	cp linux/Config.in $OUTDIR/
elif [ $VERS = 2.6 ] ;then
	echo "Copying 2.6 Kernel files"
	cp linux-core/Kconfig $OUTDIR/
fi

