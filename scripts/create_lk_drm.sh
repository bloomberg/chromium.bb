#! /bin/bash
# script to create a Linux Kernel tree from the DRM tree for diffing etc..
#
# Original author - Dave Airlie (C) 2004 - airlied@linux.ie
#

if [ $# -lt 1 ] ;then
	echo usage: $0 output_dir
	exit 1
fi

if [ ! -d shared-core -o ! -d linux-core ]  ;then
	echo not in DRM toplevel
	exit 1
fi

OUTDIR=$1/drivers/char/drm/

echo "Copying kernel independent files"
mkdir -p $OUTDIR

( cd linux-core/ ; make drm_pciids.h )
cp shared-core/*.[ch] $OUTDIR
cp linux-core/*.[ch] $OUTDIR
cp linux-core/Makefile.kernel $OUTDIR/Makefile

echo "Copying 2.6 Kernel files"
cp linux-core/Kconfig $OUTDIR/

cd $OUTDIR

rm via_ds.[ch]
for i in via*.[ch]
do
unifdef -D__linux__ -DVIA_HAVE_DMABLIT -DVIA_HAVE_CORE_MM $i > $i.tmp
mv $i.tmp $i
done

rm sis_ds.[ch]
for i in sis*.[ch]
do
unifdef -D__linux__ -DVIA_HAVE_DMABLIT -DSIS_HAVE_CORE_MM $i > $i.tmp
mv $i.tmp $i
done

for i in i915*.[ch]
do
unifdef -D__linux__ -DI915_HAVE_FENCE -DI915_HAVE_BUFFER $i > $i.tmp
mv $i.tmp $i
done

for i in drm*.[ch]
do
unifdef -UDRM_ODD_MM_COMPAT -D__linux__ $i > $i.tmp
mv $i.tmp $i
done
cd -
