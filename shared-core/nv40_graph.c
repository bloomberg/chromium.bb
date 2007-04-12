#include "drmP.h"
#include "drm.h"
#include "nouveau_drv.h"
#include "nouveau_drm.h"

/* The sizes are taken from the difference between the start of two
 * grctx addresses while running the nvidia driver.  Probably slightly
 * larger than they actually are, because of other objects being created
 * between the contexts
 */
#define NV40_GRCTX_SIZE (175*1024)
#define NV43_GRCTX_SIZE (70*1024)
#define NV46_GRCTX_SIZE (70*1024) /* probably ~64KiB */
#define NV4A_GRCTX_SIZE (64*1024)
#define NV4C_GRCTX_SIZE (25*1024)
#define NV4E_GRCTX_SIZE (25*1024)

/*TODO: deciper what each offset in the context represents. The below
 *      contexts are taken from dumps just after the 3D object is
 *      created.
 */
static void nv40_graph_context_init(drm_device_t *dev, struct mem_block *ctx)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int i;

	/* Always has the "instance address" of itself at offset 0 */
	INSTANCE_WR(ctx, 0x00000/4, nouveau_chip_instance_get(dev, ctx));
	/* unknown */
	INSTANCE_WR(ctx, 0x00024/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00028/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00030/4, 0x00000001);
	INSTANCE_WR(ctx, 0x0011c/4, 0x20010001);
	INSTANCE_WR(ctx, 0x00120/4, 0x0f73ef00);
	INSTANCE_WR(ctx, 0x00128/4, 0x02008821);
	INSTANCE_WR(ctx, 0x0016c/4, 0x00000040);
	INSTANCE_WR(ctx, 0x00170/4, 0x00000040);
	INSTANCE_WR(ctx, 0x00174/4, 0x00000040);
	INSTANCE_WR(ctx, 0x0017c/4, 0x80000000);
	INSTANCE_WR(ctx, 0x00180/4, 0x80000000);
	INSTANCE_WR(ctx, 0x00184/4, 0x80000000);
	INSTANCE_WR(ctx, 0x00188/4, 0x80000000);
	INSTANCE_WR(ctx, 0x0018c/4, 0x80000000);
	INSTANCE_WR(ctx, 0x0019c/4, 0x00000040);
	INSTANCE_WR(ctx, 0x001a0/4, 0x80000000);
	INSTANCE_WR(ctx, 0x001b0/4, 0x80000000);
	INSTANCE_WR(ctx, 0x001c0/4, 0x80000000);
	INSTANCE_WR(ctx, 0x001d0/4, 0x0b0b0b0c);
	INSTANCE_WR(ctx, 0x00340/4, 0x00040000);
	INSTANCE_WR(ctx, 0x00350/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00354/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00358/4, 0x55555555);
	INSTANCE_WR(ctx, 0x0035c/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00388/4, 0x00000008);
	INSTANCE_WR(ctx, 0x0039c/4, 0x00000010);
	INSTANCE_WR(ctx, 0x00480/4, 0x00000100);
	INSTANCE_WR(ctx, 0x00494/4, 0x00000111);
	INSTANCE_WR(ctx, 0x00498/4, 0x00080060);
	INSTANCE_WR(ctx, 0x004b4/4, 0x00000080);
	INSTANCE_WR(ctx, 0x004b8/4, 0xffff0000);
	INSTANCE_WR(ctx, 0x004bc/4, 0x00000001);
	INSTANCE_WR(ctx, 0x004d0/4, 0x46400000);
	INSTANCE_WR(ctx, 0x004ec/4, 0xffff0000);
	INSTANCE_WR(ctx, 0x004f8/4, 0x0fff0000);
	INSTANCE_WR(ctx, 0x004fc/4, 0x0fff0000);
	INSTANCE_WR(ctx, 0x00504/4, 0x00011100);
	for (i=0x00520; i<=0x0055c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x07ff0000);
	INSTANCE_WR(ctx, 0x00568/4, 0x4b7fffff);
	INSTANCE_WR(ctx, 0x00594/4, 0x30201000);
	INSTANCE_WR(ctx, 0x00598/4, 0x70605040);
	INSTANCE_WR(ctx, 0x0059c/4, 0xb8a89888);
	INSTANCE_WR(ctx, 0x005a0/4, 0xf8e8d8c8);
	INSTANCE_WR(ctx, 0x005b4/4, 0x40100000);
	INSTANCE_WR(ctx, 0x005cc/4, 0x00000004);
	INSTANCE_WR(ctx, 0x005d8/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x0060c/4, 0x435185d6);
	INSTANCE_WR(ctx, 0x00610/4, 0x2155b699);
	INSTANCE_WR(ctx, 0x00614/4, 0xfedcba98);
	INSTANCE_WR(ctx, 0x00618/4, 0x00000098);
	INSTANCE_WR(ctx, 0x00628/4, 0xffffffff);
	INSTANCE_WR(ctx, 0x0062c/4, 0x00ff7000);
	INSTANCE_WR(ctx, 0x00630/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00640/4, 0x00ff0000);
	INSTANCE_WR(ctx, 0x0067c/4, 0x00ffff00);
	/* 0x680-0x6BC - NV30_TCL_PRIMITIVE_3D_TX_ADDRESS_UNIT(0-15) */
	/* 0x6C0-0x6FC - NV30_TCL_PRIMITIVE_3D_TX_FORMAT_UNIT(0-15) */
	for (i=0x006C0; i<=0x006fc; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00018488);
	/* 0x700-0x73C - NV30_TCL_PRIMITIVE_3D_TX_WRAP_UNIT(0-15) */
	for (i=0x00700; i<=0x0073c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00028202);
	/* 0x740-0x77C - NV30_TCL_PRIMITIVE_3D_TX_ENABLE_UNIT(0-15) */
	/* 0x780-0x7BC - NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_UNIT(0-15) */
	for (i=0x00780; i<=0x007bc; i+=4)
		INSTANCE_WR(ctx, i/4, 0x0000aae4);
	/* 0x7C0-0x7FC - NV30_TCL_PRIMITIVE_3D_TX_FILTER_UNIT(0-15) */
	for (i=0x007c0; i<=0x007fc; i+=4)
		INSTANCE_WR(ctx, i/4, 0x01012000);
	/* 0x800-0x83C - NV30_TCL_PRIMITIVE_3D_TX_XY_DIM_UNIT(0-15) */
	for (i=0x00800; i<=0x0083c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00080008);
	/* 0x840-0x87C - NV30_TCL_PRIMITIVE_3D_TX_UNK07_UNIT(0-15) */
	/* 0x880-0x8BC - NV30_TCL_PRIMITIVE_3D_TX_DEPTH_UNIT(0-15) */
	for (i=0x00880; i<=0x008bc; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00100008);
	/* unknown */
	for (i=0x00910; i<=0x0091c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x0001bc80);
	for (i=0x00920; i<=0x0092c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00000202);
	for (i=0x00940; i<=0x0094c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00000008);
	for (i=0x00960; i<=0x0096c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00080008);
	INSTANCE_WR(ctx, 0x00980/4, 0x00000002);
	INSTANCE_WR(ctx, 0x009b4/4, 0x00000001);
	INSTANCE_WR(ctx, 0x009c0/4, 0x3e020200);
	INSTANCE_WR(ctx, 0x009c4/4, 0x00ffffff);
	INSTANCE_WR(ctx, 0x009c8/4, 0x60103f00);
	INSTANCE_WR(ctx, 0x009d4/4, 0x00020000);
	INSTANCE_WR(ctx, 0x00a08/4, 0x00008100);
	INSTANCE_WR(ctx, 0x00aac/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00af0/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00af8/4, 0x80800001);
	INSTANCE_WR(ctx, 0x00bcc/4, 0x00000005);
	INSTANCE_WR(ctx, 0x00bf8/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00bfc/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00c00/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00c04/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00c08/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00c0c/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00c44/4, 0x00000001);
	for (i=0x03008; i<=0x03080; i+=8)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x05288; i<=0x08570; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x08628; i<=0x08e18; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x0bd28; i<=0x0f010; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x0f0c8; i<=0x0f8b8; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x127c8; i<=0x15ab0; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x15b68; i<=0x16358; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x19268; i<=0x1c550; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x1c608; i<=0x1cdf8; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x1fd08; i<=0x22ff0; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x230a8; i<=0x23898; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x267a8; i<=0x29a90; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x29b48; i<=0x2a338; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
}

static void
nv43_graph_context_init(drm_device_t *dev, struct mem_block *ctx)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int i;
	
	INSTANCE_WR(ctx, 0x00000/4, nouveau_chip_instance_get(dev, ctx));
	INSTANCE_WR(ctx, 0x00024/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00028/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00030/4, 0x00000001);
	INSTANCE_WR(ctx, 0x0011c/4, 0x20010001);
	INSTANCE_WR(ctx, 0x00120/4, 0x0f73ef00);
	INSTANCE_WR(ctx, 0x00128/4, 0x02008821);
	INSTANCE_WR(ctx, 0x00178/4, 0x00000040);
	INSTANCE_WR(ctx, 0x0017c/4, 0x00000040);
	INSTANCE_WR(ctx, 0x00180/4, 0x00000040);
	INSTANCE_WR(ctx, 0x00188/4, 0x00000040);
	INSTANCE_WR(ctx, 0x00194/4, 0x80000000);
	INSTANCE_WR(ctx, 0x00198/4, 0x80000000);
	INSTANCE_WR(ctx, 0x0019c/4, 0x80000000);
	INSTANCE_WR(ctx, 0x001a0/4, 0x80000000);
	INSTANCE_WR(ctx, 0x001a4/4, 0x80000000);
	INSTANCE_WR(ctx, 0x001a8/4, 0x80000000);
	INSTANCE_WR(ctx, 0x001ac/4, 0x80000000);
	INSTANCE_WR(ctx, 0x001b0/4, 0x80000000);
	INSTANCE_WR(ctx, 0x001d0/4, 0x0b0b0b0c);
	INSTANCE_WR(ctx, 0x00340/4, 0x00040000);
	INSTANCE_WR(ctx, 0x00350/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00354/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00358/4, 0x55555555);
	INSTANCE_WR(ctx, 0x0035c/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00388/4, 0x00000008);
	INSTANCE_WR(ctx, 0x0039c/4, 0x00001010);
	INSTANCE_WR(ctx, 0x003cc/4, 0x00000111);
	INSTANCE_WR(ctx, 0x003d0/4, 0x00080060);
	INSTANCE_WR(ctx, 0x003ec/4, 0x00000080);
	INSTANCE_WR(ctx, 0x003f0/4, 0xffff0000);
	INSTANCE_WR(ctx, 0x003f4/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00408/4, 0x46400000);
	INSTANCE_WR(ctx, 0x00418/4, 0xffff0000);
	INSTANCE_WR(ctx, 0x00424/4, 0x0fff0000);
	INSTANCE_WR(ctx, 0x00428/4, 0x0fff0000);
	INSTANCE_WR(ctx, 0x00430/4, 0x00011100);
	for (i=0x0044c; i<=0x00488; i+=4)
		INSTANCE_WR(ctx, i/4, 0x07ff0000);
	INSTANCE_WR(ctx, 0x00494/4, 0x4b7fffff);
	INSTANCE_WR(ctx, 0x004bc/4, 0x30201000);
	INSTANCE_WR(ctx, 0x004c0/4, 0x70605040);
	INSTANCE_WR(ctx, 0x004c4/4, 0xb8a89888);
	INSTANCE_WR(ctx, 0x004c8/4, 0xf8e8d8c8);
	INSTANCE_WR(ctx, 0x004dc/4, 0x40100000);
	INSTANCE_WR(ctx, 0x004f8/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x0052c/4, 0x435185d6);
	INSTANCE_WR(ctx, 0x00530/4, 0x2155b699);
	INSTANCE_WR(ctx, 0x00534/4, 0xfedcba98);
	INSTANCE_WR(ctx, 0x00538/4, 0x00000098);
	INSTANCE_WR(ctx, 0x00548/4, 0xffffffff);
	INSTANCE_WR(ctx, 0x0054c/4, 0x00ff7000);
	INSTANCE_WR(ctx, 0x00550/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00560/4, 0x00ff0000);
	INSTANCE_WR(ctx, 0x00598/4, 0x00ffff00);
	for (i=0x005dc; i<=0x00618; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00018488);
	for (i=0x0061c; i<=0x00658; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00028202);
	for (i=0x0069c; i<=0x006d8; i+=4)
		INSTANCE_WR(ctx, i/4, 0x0000aae4);
	for (i=0x006dc; i<=0x00718; i+=4)
		INSTANCE_WR(ctx, i/4, 0x01012000);
	for (i=0x0071c; i<=0x00758; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00080008);
	for (i=0x0079c; i<=0x007d8; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00100008);
	for (i=0x0082c; i<=0x00838; i+=4)
		INSTANCE_WR(ctx, i/4, 0x0001bc80);
	for (i=0x0083c; i<=0x00848; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00000202);
	for (i=0x0085c; i<=0x00868; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00000008);
	for (i=0x0087c; i<=0x00888; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00080008);
	INSTANCE_WR(ctx, 0x0089c/4, 0x00000002);
	INSTANCE_WR(ctx, 0x008d0/4, 0x00000021);
	INSTANCE_WR(ctx, 0x008d4/4, 0x030c30c3);
	INSTANCE_WR(ctx, 0x008e0/4, 0x3e020200);
	INSTANCE_WR(ctx, 0x008e4/4, 0x00ffffff);
	INSTANCE_WR(ctx, 0x008e8/4, 0x0c103f00);
	INSTANCE_WR(ctx, 0x008f4/4, 0x00020000);
	INSTANCE_WR(ctx, 0x0092c/4, 0x00008100);
	INSTANCE_WR(ctx, 0x009b8/4, 0x00000001);
	INSTANCE_WR(ctx, 0x009fc/4, 0x00001001);
	INSTANCE_WR(ctx, 0x00a04/4, 0x00000003);
	INSTANCE_WR(ctx, 0x00a08/4, 0x00888001);
	INSTANCE_WR(ctx, 0x00a8c/4, 0x00000005);
	INSTANCE_WR(ctx, 0x00a98/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00ab4/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00ab8/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00abc/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00ac0/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00af8/4, 0x00000001);
	for (i=0x02ec0; i<=0x02f38; i+=8)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x04c80; i<=0x06e70; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x06e80; i<=0x07270; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x096c0; i<=0x0b8b0; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x0b8c0; i<=0x0bcb0; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x0e100; i<=0x102f0; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x10300; i<=0x106f0; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
};

static void nv46_graph_context_init(drm_device_t *dev, struct mem_block *ctx)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int i;

	INSTANCE_WR(ctx, 0x00000/4, nouveau_chip_instance_get(dev, ctx));
	INSTANCE_WR(ctx, 0x00040/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00044/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x0004c/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00138/4, 0x20010001);
	INSTANCE_WR(ctx, 0x0013c/4, 0x0f73ef00);
	INSTANCE_WR(ctx, 0x00144/4, 0x02008821);
	INSTANCE_WR(ctx, 0x00174/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00178/4, 0x00000001);
	INSTANCE_WR(ctx, 0x0017c/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00180/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00184/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00188/4, 0x00000001);
	INSTANCE_WR(ctx, 0x0018c/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00190/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00194/4, 0x00000040);
	INSTANCE_WR(ctx, 0x00198/4, 0x00000040);
	INSTANCE_WR(ctx, 0x0019c/4, 0x00000040);
	INSTANCE_WR(ctx, 0x001a4/4, 0x00000040);
	INSTANCE_WR(ctx, 0x001ec/4, 0x0b0b0b0c);
	INSTANCE_WR(ctx, 0x0035c/4, 0x00040000);
	INSTANCE_WR(ctx, 0x0036c/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00370/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00374/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00378/4, 0x55555555);
	INSTANCE_WR(ctx, 0x003a4/4, 0x00000008);
	INSTANCE_WR(ctx, 0x003b8/4, 0x00003010);
	INSTANCE_WR(ctx, 0x003dc/4, 0x00000111);
	INSTANCE_WR(ctx, 0x003e0/4, 0x00000111);
	INSTANCE_WR(ctx, 0x003e4/4, 0x00000111);
	INSTANCE_WR(ctx, 0x003e8/4, 0x00000111);
	INSTANCE_WR(ctx, 0x003ec/4, 0x00000111);
	INSTANCE_WR(ctx, 0x003f0/4, 0x00000111);
	INSTANCE_WR(ctx, 0x003f4/4, 0x00000111);
	INSTANCE_WR(ctx, 0x003f8/4, 0x00000111);
	INSTANCE_WR(ctx, 0x003fc/4, 0x00000111);
	INSTANCE_WR(ctx, 0x00400/4, 0x00000111);
	INSTANCE_WR(ctx, 0x00404/4, 0x00000111);
	INSTANCE_WR(ctx, 0x00408/4, 0x00000111);
	INSTANCE_WR(ctx, 0x0040c/4, 0x00000111);
	INSTANCE_WR(ctx, 0x00410/4, 0x00000111);
	INSTANCE_WR(ctx, 0x00414/4, 0x00000111);
	INSTANCE_WR(ctx, 0x00418/4, 0x00000111);
	INSTANCE_WR(ctx, 0x004b0/4, 0x00000111);
	INSTANCE_WR(ctx, 0x004b4/4, 0x00080060);
	INSTANCE_WR(ctx, 0x004d0/4, 0x00000080);
	INSTANCE_WR(ctx, 0x004d4/4, 0xffff0000);
	INSTANCE_WR(ctx, 0x004d8/4, 0x00000001);
	INSTANCE_WR(ctx, 0x004ec/4, 0x46400000);
	INSTANCE_WR(ctx, 0x004fc/4, 0xffff0000);
	INSTANCE_WR(ctx, 0x00500/4, 0x88888888);
	INSTANCE_WR(ctx, 0x00504/4, 0x88888888);
	INSTANCE_WR(ctx, 0x00508/4, 0x88888888);
	INSTANCE_WR(ctx, 0x0050c/4, 0x88888888);
	INSTANCE_WR(ctx, 0x00510/4, 0x88888888);
	INSTANCE_WR(ctx, 0x00514/4, 0x88888888);
	INSTANCE_WR(ctx, 0x00518/4, 0x88888888);
	INSTANCE_WR(ctx, 0x0051c/4, 0x88888888);
	INSTANCE_WR(ctx, 0x00520/4, 0x88888888);
	INSTANCE_WR(ctx, 0x00524/4, 0x88888888);
	INSTANCE_WR(ctx, 0x00528/4, 0x88888888);
	INSTANCE_WR(ctx, 0x0052c/4, 0x88888888);
	INSTANCE_WR(ctx, 0x00530/4, 0x88888888);
	INSTANCE_WR(ctx, 0x00534/4, 0x88888888);
	INSTANCE_WR(ctx, 0x00538/4, 0x88888888);
	INSTANCE_WR(ctx, 0x0053c/4, 0x88888888);
	INSTANCE_WR(ctx, 0x00550/4, 0x0fff0000);
	INSTANCE_WR(ctx, 0x00554/4, 0x0fff0000);
	INSTANCE_WR(ctx, 0x0055c/4, 0x00011100);
	for (i=0x00578; i<0x005b4; i+=4)
		INSTANCE_WR(ctx, i/4, 0x07ff0000);
	INSTANCE_WR(ctx, 0x005c0/4, 0x4b7fffff);
	INSTANCE_WR(ctx, 0x005e8/4, 0x30201000);
	INSTANCE_WR(ctx, 0x005ec/4, 0x70605040);
	INSTANCE_WR(ctx, 0x005f0/4, 0xb8a89888);
	INSTANCE_WR(ctx, 0x005f4/4, 0xf8e8d8c8);
	INSTANCE_WR(ctx, 0x00608/4, 0x40100000);
	INSTANCE_WR(ctx, 0x00624/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00658/4, 0x435185d6);
	INSTANCE_WR(ctx, 0x0065c/4, 0x2155b699);
	INSTANCE_WR(ctx, 0x00660/4, 0xfedcba98);
	INSTANCE_WR(ctx, 0x00664/4, 0x00000098);
	INSTANCE_WR(ctx, 0x00674/4, 0xffffffff);
	INSTANCE_WR(ctx, 0x00678/4, 0x00ff7000);
	INSTANCE_WR(ctx, 0x0067c/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x0068c/4, 0x00ff0000);
	INSTANCE_WR(ctx, 0x006c8/4, 0x00ffff00);
	for (i=0x0070c; i<=0x00748; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00018488);
	for (i=0x0074c; i<=0x00788; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00028202);
	for (i=0x007cc; i<=0x00808; i+=4)
		INSTANCE_WR(ctx, i/4, 0x0000aae4);
	for (i=0x0080c; i<=0x00848; i+=4)
		INSTANCE_WR(ctx, i/4, 0x01012000);
	for (i=0x0084c; i<=0x00888; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00080008);
	for (i=0x008cc; i<=0x00908; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00100008);
	for (i=0x0095c; i<=0x00968; i+=4)
		INSTANCE_WR(ctx, i/4, 0x0001bc80);
	for (i=0x0096c; i<=0x00978; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00000202);
	for (i=0x0098c; i<=0x00998; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00000008);
	for (i=0x009ac; i<=0x009b8; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00080008);
	INSTANCE_WR(ctx, 0x009cc/4, 0x00000002);
	INSTANCE_WR(ctx, 0x00a00/4, 0x00000421);
	INSTANCE_WR(ctx, 0x00a04/4, 0x030c30c3);
	INSTANCE_WR(ctx, 0x00a08/4, 0x00011001);
	INSTANCE_WR(ctx, 0x00a14/4, 0x3e020200);
	INSTANCE_WR(ctx, 0x00a18/4, 0x00ffffff);
	INSTANCE_WR(ctx, 0x00a1c/4, 0x0c103f00);
	INSTANCE_WR(ctx, 0x00a28/4, 0x00040000);
	INSTANCE_WR(ctx, 0x00a60/4, 0x00008100);
	INSTANCE_WR(ctx, 0x00aec/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00b30/4, 0x00001001);
	INSTANCE_WR(ctx, 0x00b38/4, 0x00000003);
	INSTANCE_WR(ctx, 0x00b3c/4, 0x00888001);
	INSTANCE_WR(ctx, 0x00bc0/4, 0x00000005);
	INSTANCE_WR(ctx, 0x00bcc/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00be8/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00bec/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00bf0/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00bf4/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00c2c/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00c30/4, 0x08e00001);
	INSTANCE_WR(ctx, 0x00c34/4, 0x000e3000);
	for (i=0x017f8; i<=0x01870; i+=8)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x035b8; i<=0x057a8; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x057b8; i<=0x05ba8; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x07f38; i<=0x0a128; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x0a138; i<=0x0a528; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x0c8b8; i<=0x0eaa8; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x0eab8; i<=0x0eea8; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
}

static void nv4a_graph_context_init(drm_device_t *dev, struct mem_block *ctx)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int i;

	INSTANCE_WR(ctx, 0x00000/4, nouveau_chip_instance_get(dev, ctx));
	INSTANCE_WR(ctx, 0x00024/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00028/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00030/4, 0x00000001);
	INSTANCE_WR(ctx, 0x0011c/4, 0x20010001);
	INSTANCE_WR(ctx, 0x00120/4, 0x0f73ef00);
	INSTANCE_WR(ctx, 0x00128/4, 0x02008821);
	INSTANCE_WR(ctx, 0x00158/4, 0x00000001);
	INSTANCE_WR(ctx, 0x0015c/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00160/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00164/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00168/4, 0x00000001);
	INSTANCE_WR(ctx, 0x0016c/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00170/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00174/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00178/4, 0x00000040);
	INSTANCE_WR(ctx, 0x0017c/4, 0x00000040);
	INSTANCE_WR(ctx, 0x00180/4, 0x00000040);
	INSTANCE_WR(ctx, 0x00188/4, 0x00000040);
	INSTANCE_WR(ctx, 0x001d0/4, 0x0b0b0b0c);
	INSTANCE_WR(ctx, 0x00340/4, 0x00040000);
	INSTANCE_WR(ctx, 0x00350/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00354/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00358/4, 0x55555555);
	INSTANCE_WR(ctx, 0x0035c/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00388/4, 0x00000008);
	INSTANCE_WR(ctx, 0x0039c/4, 0x00003010);
	INSTANCE_WR(ctx, 0x003cc/4, 0x00000111);
	INSTANCE_WR(ctx, 0x003d0/4, 0x00080060);
	INSTANCE_WR(ctx, 0x003ec/4, 0x00000080);
	INSTANCE_WR(ctx, 0x003f0/4, 0xffff0000);
	INSTANCE_WR(ctx, 0x003f4/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00408/4, 0x46400000);
	INSTANCE_WR(ctx, 0x00418/4, 0xffff0000);
	INSTANCE_WR(ctx, 0x00424/4, 0x0fff0000);
	INSTANCE_WR(ctx, 0x00428/4, 0x0fff0000);
	INSTANCE_WR(ctx, 0x00430/4, 0x00011100);
	for (i=0x0044c; i<=0x00488; i+=4)
		INSTANCE_WR(ctx, i/4, 0x07ff0000);
	INSTANCE_WR(ctx, 0x00494/4, 0x4b7fffff);
	INSTANCE_WR(ctx, 0x004bc/4, 0x30201000);
	INSTANCE_WR(ctx, 0x004c0/4, 0x70605040);
	INSTANCE_WR(ctx, 0x004c4/4, 0xb8a89888);
	INSTANCE_WR(ctx, 0x004c8/4, 0xf8e8d8c8);
	INSTANCE_WR(ctx, 0x004dc/4, 0x40100000);
	INSTANCE_WR(ctx, 0x004f8/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x0052c/4, 0x435185d6);
	INSTANCE_WR(ctx, 0x00530/4, 0x2155b699);
	INSTANCE_WR(ctx, 0x00534/4, 0xfedcba98);
	INSTANCE_WR(ctx, 0x00538/4, 0x00000098);
	INSTANCE_WR(ctx, 0x00548/4, 0xffffffff);
	INSTANCE_WR(ctx, 0x0054c/4, 0x00ff7000);
	INSTANCE_WR(ctx, 0x00550/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x0055c/4, 0x00ff0000);
	INSTANCE_WR(ctx, 0x00594/4, 0x00ffff00);
	for (i=0x005d8; i<=0x00614; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00018488);
	for (i=0x00618; i<=0x00654; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00028202);
	for (i=0x00698; i<=0x006d4; i+=4)
		INSTANCE_WR(ctx, i/4, 0x0000aae4);
	for (i=0x006d8; i<=0x00714; i+=4)
		INSTANCE_WR(ctx, i/4, 0x01012000);
	for (i=0x00718; i<=0x00754; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00080008);
	for (i=0x00798; i<=0x007d4; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00100008);
	for (i=0x00828; i<=0x00834; i+=4)
		INSTANCE_WR(ctx, i/4, 0x0001bc80);
	for (i=0x00838; i<=0x00844; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00000202);
	for (i=0x00858; i<=0x00864; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00000008);
	for (i=0x00878; i<=0x00884; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00080008);
	INSTANCE_WR(ctx, 0x00898/4, 0x00000002);
	INSTANCE_WR(ctx, 0x008cc/4, 0x00000021);
	INSTANCE_WR(ctx, 0x008d0/4, 0x030c30c3);
	INSTANCE_WR(ctx, 0x008d4/4, 0x00011001);
	INSTANCE_WR(ctx, 0x008e0/4, 0x3e020200);
	INSTANCE_WR(ctx, 0x008e4/4, 0x00ffffff);
	INSTANCE_WR(ctx, 0x008e8/4, 0x0c103f00);
	INSTANCE_WR(ctx, 0x008f4/4, 0x00040000);
	INSTANCE_WR(ctx, 0x0092c/4, 0x00008100);
	INSTANCE_WR(ctx, 0x009b8/4, 0x00000001);
	INSTANCE_WR(ctx, 0x009fc/4, 0x00001001);
	INSTANCE_WR(ctx, 0x00a04/4, 0x00000003);
	INSTANCE_WR(ctx, 0x00a08/4, 0x00888001);
	INSTANCE_WR(ctx, 0x00a8c/4, 0x00000005);
	INSTANCE_WR(ctx, 0x00a98/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00ab4/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00ab8/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00abc/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00ac0/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00af8/4, 0x00000001);
	for (i=0x016c0; i<=0x01738; i+=8)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x03840; i<=0x05670; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x05680; i<=0x05a70; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x07e00; i<=0x09ff0; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x0a000; i<=0x0a3f0; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x0c780; i<=0x0e970; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x0e980; i<=0x0ed70; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
}


static void nv4c_graph_context_init(drm_device_t *dev, struct mem_block *ctx)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int i;

	INSTANCE_WR(ctx, 0x00000/4, nouveau_chip_instance_get(dev, ctx));
	INSTANCE_WR(ctx, 0x00024/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00028/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00030/4, 0x00000001);
	INSTANCE_WR(ctx, 0x0011c/4, 0x20010001);
	INSTANCE_WR(ctx, 0x00120/4, 0x0f73ef00);
	INSTANCE_WR(ctx, 0x00128/4, 0x02008821);
	INSTANCE_WR(ctx, 0x00158/4, 0x00000001);
	INSTANCE_WR(ctx, 0x0015c/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00160/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00164/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00168/4, 0x00000001);
	INSTANCE_WR(ctx, 0x0016c/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00170/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00174/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00178/4, 0x00000040);
	INSTANCE_WR(ctx, 0x0017c/4, 0x00000040);
	INSTANCE_WR(ctx, 0x00180/4, 0x00000040);
	INSTANCE_WR(ctx, 0x00188/4, 0x00000040);
	INSTANCE_WR(ctx, 0x001d0/4, 0x0b0b0b0c);
	INSTANCE_WR(ctx, 0x00340/4, 0x00040000);
	INSTANCE_WR(ctx, 0x00350/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00354/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00358/4, 0x55555555);
	INSTANCE_WR(ctx, 0x0035c/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00388/4, 0x00000008);
	INSTANCE_WR(ctx, 0x0039c/4, 0x00001010);
	INSTANCE_WR(ctx, 0x003d0/4, 0x00000111);
	INSTANCE_WR(ctx, 0x003d4/4, 0x00080060);
	INSTANCE_WR(ctx, 0x003f0/4, 0x00000080);
	INSTANCE_WR(ctx, 0x003f4/4, 0xffff0000);
	INSTANCE_WR(ctx, 0x003f8/4, 0x00000001);
	INSTANCE_WR(ctx, 0x0040c/4, 0x46400000);
	INSTANCE_WR(ctx, 0x0041c/4, 0xffff0000);
	INSTANCE_WR(ctx, 0x00428/4, 0x0fff0000);
	INSTANCE_WR(ctx, 0x0042c/4, 0x0fff0000);
	INSTANCE_WR(ctx, 0x00434/4, 0x00011100);
	for (i=0x00450; i<0x0048c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x07ff0000);
	INSTANCE_WR(ctx, 0x00498/4, 0x4b7fffff);
	INSTANCE_WR(ctx, 0x004c0/4, 0x30201000);
	INSTANCE_WR(ctx, 0x004c4/4, 0x70605040);
	INSTANCE_WR(ctx, 0x004c8/4, 0xb8a89888);
	INSTANCE_WR(ctx, 0x004cc/4, 0xf8e8d8c8);
	INSTANCE_WR(ctx, 0x004e0/4, 0x40100000);
	INSTANCE_WR(ctx, 0x004fc/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00530/4, 0x435185d6);
	INSTANCE_WR(ctx, 0x00534/4, 0x2155b699);
	INSTANCE_WR(ctx, 0x00538/4, 0xfedcba98);
	INSTANCE_WR(ctx, 0x0053c/4, 0x00000098);
	INSTANCE_WR(ctx, 0x0054c/4, 0xffffffff);
	INSTANCE_WR(ctx, 0x00550/4, 0x00ff7000);
	INSTANCE_WR(ctx, 0x00554/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00564/4, 0x00ff0000);
	INSTANCE_WR(ctx, 0x0059c/4, 0x00ffff00);
	for (i=0x005e0; i<=0x0061c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00018488);
	for (i=0x00620; i<=0x0065c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00028202);
	for (i=0x006a0; i<=0x006dc; i+=4)
		INSTANCE_WR(ctx, i/4, 0x0000aae4);
	for (i=0x006e0; i<=0x0071c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x01012000);
	for (i=0x00720; i<=0x0075c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00080008);
	for (i=0x007a0; i<=0x007dc; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00100008);
	for (i=0x00830; i<=0x0083c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x0001bc80);
	for (i=0x00840; i<=0x0084c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00000202);
	for (i=0x00860; i<=0x0086c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00000008);
	for (i=0x00880; i<=0x0088c; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00080008);
	INSTANCE_WR(ctx, 0x008a0/4, 0x00000002);
	INSTANCE_WR(ctx, 0x008d4/4, 0x00000020);
	INSTANCE_WR(ctx, 0x008d8/4, 0x030c30c3);
	INSTANCE_WR(ctx, 0x008dc/4, 0x00011001);
	INSTANCE_WR(ctx, 0x008e8/4, 0x3e020200);
	INSTANCE_WR(ctx, 0x008ec/4, 0x00ffffff);
	INSTANCE_WR(ctx, 0x008f0/4, 0x0c103f00);
	INSTANCE_WR(ctx, 0x008fc/4, 0x00040000);
	INSTANCE_WR(ctx, 0x00934/4, 0x00008100);
	INSTANCE_WR(ctx, 0x009c0/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00a04/4, 0x00001001);
	INSTANCE_WR(ctx, 0x00a0c/4, 0x00000003);
	INSTANCE_WR(ctx, 0x00a10/4, 0x00888001);
	INSTANCE_WR(ctx, 0x00a74/4, 0x00000005);
	INSTANCE_WR(ctx, 0x00a80/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00a9c/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00aa0/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00ad8/4, 0x00000001);
	for (i=0x016a0; i<0x01718; i+=8)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x03460; i<0x05650; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x05660; i<0x05a50; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
}

static void nv4e_graph_context_init(drm_device_t *dev, struct mem_block *ctx)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int i;

	INSTANCE_WR(ctx, 0x00000/4, nouveau_chip_instance_get(dev, ctx));
	INSTANCE_WR(ctx, 0x00024/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00028/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00030/4, 0x00000001);
	INSTANCE_WR(ctx, 0x0011c/4, 0x20010001);
	INSTANCE_WR(ctx, 0x00120/4, 0x0f73ef00);
	INSTANCE_WR(ctx, 0x00128/4, 0x02008821);
	INSTANCE_WR(ctx, 0x00158/4, 0x00000001);
	INSTANCE_WR(ctx, 0x0015c/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00160/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00164/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00168/4, 0x00000001);
	INSTANCE_WR(ctx, 0x0016c/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00170/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00174/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00178/4, 0x00000040);
	INSTANCE_WR(ctx, 0x0017c/4, 0x00000040);
	INSTANCE_WR(ctx, 0x00180/4, 0x00000040);
	INSTANCE_WR(ctx, 0x00188/4, 0x00000040);
	INSTANCE_WR(ctx, 0x001d0/4, 0x0b0b0b0c);
	INSTANCE_WR(ctx, 0x00340/4, 0x00040000);
	INSTANCE_WR(ctx, 0x00350/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00354/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00358/4, 0x55555555);
	INSTANCE_WR(ctx, 0x0035c/4, 0x55555555);
	INSTANCE_WR(ctx, 0x00388/4, 0x00000008);
	INSTANCE_WR(ctx, 0x0039c/4, 0x00001010);
	INSTANCE_WR(ctx, 0x003cc/4, 0x00000111);
	INSTANCE_WR(ctx, 0x003d0/4, 0x00080060);
	INSTANCE_WR(ctx, 0x003ec/4, 0x00000080);
	INSTANCE_WR(ctx, 0x003f0/4, 0xffff0000);
	INSTANCE_WR(ctx, 0x003f4/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00408/4, 0x46400000);
	INSTANCE_WR(ctx, 0x00418/4, 0xffff0000);
	INSTANCE_WR(ctx, 0x00424/4, 0x0fff0000);
	INSTANCE_WR(ctx, 0x00428/4, 0x0fff0000);
	INSTANCE_WR(ctx, 0x00430/4, 0x00011100);
	for (i=0x0044c; i<=0x00488; i+=4)
		INSTANCE_WR(ctx, i/4, 0x07ff0000);
	INSTANCE_WR(ctx, 0x00494/4, 0x4b7fffff);
	INSTANCE_WR(ctx, 0x004bc/4, 0x30201000);
	INSTANCE_WR(ctx, 0x004c0/4, 0x70605040);
	INSTANCE_WR(ctx, 0x004c4/4, 0xb8a89888);
	INSTANCE_WR(ctx, 0x004c8/4, 0xf8e8d8c8);
	INSTANCE_WR(ctx, 0x004dc/4, 0x40100000);
	INSTANCE_WR(ctx, 0x004f8/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x0052c/4, 0x435185d6);
	INSTANCE_WR(ctx, 0x00530/4, 0x2155b699);
	INSTANCE_WR(ctx, 0x00534/4, 0xfedcba98);
	INSTANCE_WR(ctx, 0x00538/4, 0x00000098);
	INSTANCE_WR(ctx, 0x00548/4, 0xffffffff);
	INSTANCE_WR(ctx, 0x0054c/4, 0x00ff7000);
	INSTANCE_WR(ctx, 0x00550/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x0055c/4, 0x00ff0000);
	INSTANCE_WR(ctx, 0x00594/4, 0x00ffff00);
	for (i=0x005d8; i<=0x00614; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00018488);
	for (i=0x00618; i<=0x00654; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00028202);
	for (i=0x00698; i<=0x006d4; i+=4)
		INSTANCE_WR(ctx, i/4, 0x0000aae4);
	for (i=0x006d8; i<=0x00714; i+=4)
		INSTANCE_WR(ctx, i/4, 0x01012000);
	for (i=0x00718; i<=0x00754; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00080008);
	for (i=0x00798; i<=0x007d4; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00100008);
	for (i=0x00828; i<=0x00834; i+=4)
		INSTANCE_WR(ctx, i/4, 0x0001bc80);
	for (i=0x00838; i<=0x00844; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00000202);
	for (i=0x00858; i<=0x00864; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00000008);
	for (i=0x00878; i<=0x00884; i+=4)
		INSTANCE_WR(ctx, i/4, 0x00080008);
	INSTANCE_WR(ctx, 0x00898/4, 0x00000002);
	INSTANCE_WR(ctx, 0x008cc/4, 0x00000020);
	INSTANCE_WR(ctx, 0x008d0/4, 0x030c30c3);
	INSTANCE_WR(ctx, 0x008d4/4, 0x00011001);
	INSTANCE_WR(ctx, 0x008e0/4, 0x3e020200);
	INSTANCE_WR(ctx, 0x008e4/4, 0x00ffffff);
	INSTANCE_WR(ctx, 0x008e8/4, 0x0c103f00);
	INSTANCE_WR(ctx, 0x008f4/4, 0x00040000);
	INSTANCE_WR(ctx, 0x0092c/4, 0x00008100);
	INSTANCE_WR(ctx, 0x009b8/4, 0x00000001);
	INSTANCE_WR(ctx, 0x009fc/4, 0x00001001);
	INSTANCE_WR(ctx, 0x00a04/4, 0x00000003);
	INSTANCE_WR(ctx, 0x00a08/4, 0x00888001);
	INSTANCE_WR(ctx, 0x00a6c/4, 0x00000005);
	INSTANCE_WR(ctx, 0x00a78/4, 0x0000ffff);
	INSTANCE_WR(ctx, 0x00a94/4, 0x00005555);
	INSTANCE_WR(ctx, 0x00a98/4, 0x00000001);
	INSTANCE_WR(ctx, 0x00aa4/4, 0x00000001);
	for (i=0x01668; i<=0x016e0; i+=8)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
	for (i=0x03428; i<=0x05618; i+=24)
		INSTANCE_WR(ctx, i/4, 0x00000001);
	for (i=0x05628; i<=0x05a18; i+=16)
		INSTANCE_WR(ctx, i/4, 0x3f800000);
}

int
nv40_graph_context_create(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv =
		(drm_nouveau_private_t *)dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[channel];
	void (*ctx_init)(drm_device_t *, struct mem_block *);
	unsigned int ctx_size;
	int i;

	switch (dev_priv->chipset) {
	case 0x40:
		ctx_size = NV40_GRCTX_SIZE;
		ctx_init = nv40_graph_context_init;
		break;
	case 0x43:
		ctx_size = NV43_GRCTX_SIZE;
		ctx_init = nv43_graph_context_init;
		break;
	case 0x46:
		ctx_size = NV46_GRCTX_SIZE;
		ctx_init = nv46_graph_context_init;
		break;
	case 0x4a:
		ctx_size = NV4A_GRCTX_SIZE;
		ctx_init = nv4a_graph_context_init;
		break;
	case 0x4c:
		ctx_size = NV4C_GRCTX_SIZE;
		ctx_init = nv4c_graph_context_init;
		break;
	case 0x4e:
		ctx_size = NV4E_GRCTX_SIZE;
		ctx_init = nv4e_graph_context_init;
		break;
	default:
		ctx_size = NV40_GRCTX_SIZE;
		ctx_init = nv40_graph_context_init;
		break;
	}

	/* Alloc and clear RAMIN to store the context */
	chan->ramin_grctx = nouveau_instmem_alloc(dev, ctx_size, 4);
	if (!chan->ramin_grctx)
		return DRM_ERR(ENOMEM);
	for (i=0; i<ctx_size; i+=4)
		INSTANCE_WR(chan->ramin_grctx, i/4, 0x00000000);

	/* Initialise default context values */
	ctx_init(dev, chan->ramin_grctx);

	return 0;
}

/* Save current context (from PGRAPH) into the channel's context
 *XXX: fails sometimes, not sure why..
 */
void
nv40_graph_context_save_current(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv =
		(drm_nouveau_private_t *)dev->dev_private;
	uint32_t instance;
	int i;

	NV_WRITE(NV04_PGRAPH_FIFO, 0);

	instance = NV_READ(0x40032C) & 0xFFFFF;
	if (!instance) {
		NV_WRITE(NV04_PGRAPH_FIFO, 1);
		return;
	}

	NV_WRITE(0x400784, instance);
	NV_WRITE(0x400310, NV_READ(0x400310) | 0x20);
	NV_WRITE(0x400304, 1);
	/* just in case, we don't want to spin in-kernel forever */
	for (i=0; i<1000; i++) {
		if (NV_READ(0x40030C) == 0)
			break;
	}
	if (i==1000) {
		DRM_ERROR("failed to save current grctx to ramin\n");
		DRM_ERROR("instance = 0x%08x\n", NV_READ(0x40032C));
		DRM_ERROR("0x40030C = 0x%08x\n", NV_READ(0x40030C));
		NV_WRITE(NV04_PGRAPH_FIFO, 1);
		return;
	}

	NV_WRITE(NV04_PGRAPH_FIFO, 1);
}

/* Restore the context for a specific channel into PGRAPH
 * XXX: fails sometimes.. not sure why
 */
void
nv40_graph_context_restore(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv =
		(drm_nouveau_private_t *)dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[channel];
	uint32_t instance;
	int i;

	instance = nouveau_chip_instance_get(dev, chan->ramin_grctx);

	NV_WRITE(NV04_PGRAPH_FIFO, 0);
	NV_WRITE(0x400784, instance);
	NV_WRITE(0x400310, NV_READ(0x400310) | 0x40);
	NV_WRITE(0x400304, 1);
	/* just in case, we don't want to spin in-kernel forever */
	for (i=0; i<1000; i++) {
		if (NV_READ(0x40030C) == 0)
			break;
	}
	if (i==1000) {
		DRM_ERROR("failed to restore grctx for ch%d to PGRAPH\n",
				channel);
		DRM_ERROR("instance = 0x%08x\n", instance);
		DRM_ERROR("0x40030C = 0x%08x\n", NV_READ(0x40030C));
		NV_WRITE(NV04_PGRAPH_FIFO, 1);
		return;
	}


	/* 0x40032C, no idea of it's exact function.  Could simply be a
	 * record of the currently active PGRAPH context.  It's currently
	 * unknown as to what bit 24 does.  The nv ddx has it set, so we will
	 * set it here too.
	 */
	NV_WRITE(0x40032C, instance | 0x01000000);
	/* 0x32E0 records the instance address of the active FIFO's PGRAPH
	 * context.  If at any time this doesn't match 0x40032C, you will
	 * recieve PGRAPH_INTR_CONTEXT_SWITCH
	 */
	NV_WRITE(NV40_PFIFO_GRCTX_INSTANCE, instance);
	NV_WRITE(NV04_PGRAPH_FIFO, 1);
}

/* Some voodoo that makes context switching work without the binary driver
 * initialising the card first.
 *
 * It is possible to effect how the context is saved from PGRAPH into a block
 * of instance memory by altering the values in these tables.  This may mean
 * that the context layout of each chipset is slightly different (at least
 * NV40 and C51 are different).  It would also be possible for chipsets to
 * have an identical context layout, but pull the data from different PGRAPH
 * registers.
 *
 * TODO: decode the meaning of the magic values, may provide clues about the
 *       differences between the various NV40 chipsets.
 * TODO: one we have a better idea of how each chipset differs, perhaps think
 *       about unifying these instead of providing a separate table for each
 *       chip.
 *
 * mmio-trace dumps from other nv4x/g7x/c5x cards very welcome :)
 */
static uint32_t nv40_ctx_voodoo[] = {
	0x00400889, 0x00200000, 0x0060000a, 0x00200000, 0x00300000, 0x00800001,
	0x00700009, 0x0060000e, 0x00400d64, 0x00400d05, 0x00408f65, 0x00409406,
	0x0040a268, 0x00200000, 0x0060000a, 0x00700000, 0x00106000, 0x00700080,
	0x004014e6, 0x007000a0, 0x00401a84, 0x00700082, 0x00600001, 0x00500061,
	0x00600002, 0x00401b68, 0x00500060, 0x00200001, 0x0060000a, 0x0011814d,
	0x00110158, 0x00105401, 0x0020003a, 0x00100051, 0x001040c5, 0x0010c1c4,
	0x001041c9, 0x0010c1dc, 0x00110205, 0x0011420a, 0x00114210, 0x00110216,
	0x0012421b, 0x00120270, 0x001242c0, 0x00200040, 0x00100280, 0x00128100,
	0x00128120, 0x00128143, 0x0011415f, 0x0010815c, 0x0010c140, 0x00104029,
	0x00110400, 0x00104d10, 0x00500060, 0x00403b87, 0x0060000d, 0x004076e6,
	0x002000f0, 0x0060000a, 0x00200045, 0x00100620, 0x00108668, 0x0011466b,
	0x00120682, 0x0011068b, 0x00168691, 0x0010c6ae, 0x001206b4, 0x0020002a,
	0x001006c4, 0x001246f0, 0x002000c0, 0x00100700, 0x0010c3d7, 0x001043e1,
	0x00500060, 0x00405600, 0x00405684, 0x00600003, 0x00500067, 0x00600008,
	0x00500060, 0x00700082, 0x0020026c, 0x0060000a, 0x00104800, 0x00104901,
	0x00120920, 0x00200035, 0x00100940, 0x00148a00, 0x00104a14, 0x00200038,
	0x00100b00, 0x00138d00, 0x00104e00, 0x0012d600, 0x00105c00, 0x00104f06,
	0x0020031a, 0x0060000a, 0x00300000, 0x00200680, 0x00406c00, 0x00200684,
	0x00800001, 0x00200b62, 0x0060000a, 0x0020a0b0, 0x0040728a, 0x00201b68,
	0x00800041, 0x00407684, 0x00203e60, 0x00800002, 0x00408700, 0x00600006,
	0x00700003, 0x004080e6, 0x00700080, 0x0020031a, 0x0060000a, 0x00200004,
	0x00800001, 0x00700000, 0x00200000, 0x0060000a, 0x00106002, 0x0040a284,
	0x00700002, 0x00600004, 0x0040a268, 0x00700000, 0x00200000, 0x0060000a,
	0x00106002, 0x00700080, 0x00400a84, 0x00700002, 0x00400a68, 0x00500060,
	0x00600007, 0x00409388, 0x0060000f, 0x00000000, 0x00500060, 0x00200000,
	0x0060000a, 0x00700000, 0x00106001, 0x00700083, 0x00910880, 0x00901ffe,
	0x00940400, 0x00200020, 0x0060000b, 0x00500069, 0x0060000c, 0x00401b68,
	0x0040a406, 0x0040a505, 0x00600009, 0x00700005, 0x00700006, 0x0060000e,
	~0
};

static uint32_t nv43_ctx_voodoo[] = {
	0x00400889, 0x00200000, 0x0060000a, 0x00200000, 0x00300000, 0x00800001,
	0x00700009, 0x0060000e, 0x00400d64, 0x00400d05, 0x00409565, 0x00409a06,
	0x0040a868, 0x00200000, 0x0060000a, 0x00700000, 0x00106000, 0x00700080,
	0x004014e6, 0x007000a0, 0x00401a84, 0x00700082, 0x00600001, 0x00500061,
	0x00600002, 0x00401b68, 0x00500060, 0x00200001, 0x0060000a, 0x0011814d,
	0x00110158, 0x00105401, 0x0020003a, 0x00100051, 0x001040c5, 0x0010c1c4,
	0x001041c9, 0x0010c1dc, 0x00150210, 0x0012c225, 0x00108238, 0x0010823e,
	0x001242c0, 0x00200040, 0x00100280, 0x00128100, 0x00128120, 0x00128143,
	0x0011415f, 0x0010815c, 0x0010c140, 0x00104029, 0x00110400, 0x00104d10,
	0x001046ec, 0x00500060, 0x00403a87, 0x0060000d, 0x00407ce6, 0x002000f1,
	0x0060000a, 0x00148653, 0x00104668, 0x0010c66d, 0x00120682, 0x0011068b,
	0x00168691, 0x001046ae, 0x001046b0, 0x001206b4, 0x001046c4, 0x001146c6,
	0x00200020, 0x001006cc, 0x001046ed, 0x001246f0, 0x002000c0, 0x00100700,
	0x0010c3d7, 0x001043e1, 0x00500060, 0x00405800, 0x00405884, 0x00600003,
	0x00500067, 0x00600008, 0x00500060, 0x00700082, 0x00200233, 0x0060000a,
	0x00104800, 0x00108901, 0x00124920, 0x0020001f, 0x00100940, 0x00140965,
	0x00148a00, 0x00108a14, 0x00160b00, 0x00134b2c, 0x0010cd00, 0x0010cd04,
	0x0010cd08, 0x00104d80, 0x00104e00, 0x0012d600, 0x00105c00, 0x00104f06,
	0x002002c8, 0x0060000a, 0x00300000, 0x00200680, 0x00407200, 0x00200684,
	0x00800001, 0x00200b10, 0x0060000a, 0x00203870, 0x0040788a, 0x00201350,
	0x00800041, 0x00407c84, 0x00201560, 0x00800002, 0x00408d00, 0x00600006,
	0x00700003, 0x004086e6, 0x00700080, 0x002002c8, 0x0060000a, 0x00200004,
	0x00800001, 0x00700000, 0x00200000, 0x0060000a, 0x00106002, 0x0040a884,
	0x00700002, 0x00600004, 0x0040a868, 0x00700000, 0x00200000, 0x0060000a,
	0x00106002, 0x00700080, 0x00400a84, 0x00700002, 0x00400a68, 0x00500060,
	0x00600007, 0x00409988, 0x0060000f, 0x00000000, 0x00500060, 0x00200000,
	0x0060000a, 0x00700000, 0x00106001, 0x00700083, 0x00910880, 0x00901ffe,
	0x00940400, 0x00200020, 0x0060000b, 0x00500069, 0x0060000c, 0x00401b68,
	0x0040aa06, 0x0040ab05, 0x00600009, 0x00700005, 0x00700006, 0x0060000e,
	~0
};

static uint32_t nv46_ctx_voodoo[] = {
	0x00400889, 0x00200000, 0x0060000a, 0x00200000, 0x00300000, 0x00800001,
	0x00700009, 0x0060000e, 0x00400d64, 0x00400d05, 0x00408f65, 0x00409306,
	0x0040a068, 0x0040198f, 0x00200001, 0x0060000a, 0x00700080, 0x00104042,
	0x00200001, 0x0060000a, 0x00700000, 0x001040c5, 0x00401826, 0x00401968,
	0x0060000d, 0x00200000, 0x0060000a, 0x00700000, 0x00106000, 0x00700080,
	0x004020e6, 0x007000a0, 0x00500060, 0x00200008, 0x0060000a, 0x0011814d,
	0x00110158, 0x00105401, 0x0020003a, 0x00100051, 0x001040c5, 0x0010c1c4,
	0x001041c9, 0x0010c1dc, 0x00150210, 0x0012c225, 0x00108238, 0x0010823e,
	0x001242c0, 0x00200040, 0x00100280, 0x00128100, 0x00128120, 0x00128143,
	0x0011415f, 0x0010815c, 0x0010c140, 0x00104029, 0x00110400, 0x00104d10,
	0x00500060, 0x00403f87, 0x0060000d, 0x004079e6, 0x002000f7, 0x0060000a,
	0x00200045, 0x00100620, 0x00104668, 0x0017466d, 0x0011068b, 0x00168691,
	0x001046ae, 0x001046b0, 0x001206b4, 0x001046c4, 0x001146c6, 0x00200022,
	0x001006cc, 0x001246f0, 0x002000c0, 0x00100700, 0x0010c3d7, 0x001043e1,
	0x00500060, 0x0020027f, 0x0060000a, 0x00104800, 0x00108901, 0x00104910,
	0x00124920, 0x0020001f, 0x00100940, 0x00140965, 0x00148a00, 0x00108a14,
	0x00160b00, 0x00134b2c, 0x0010cd00, 0x0010cd04, 0x0010cd08, 0x00104d80,
	0x00104e00, 0x0012d600, 0x00105c00, 0x00104f06, 0x00105406, 0x00105709,
	0x00200316, 0x0060000a, 0x00300000, 0x00200080, 0x00407200, 0x00200084,
	0x00800001, 0x0020055e, 0x0060000a, 0x002037e0, 0x0040788a, 0x00201320,
	0x00800029, 0x00408900, 0x00600006, 0x004085e6, 0x00700080, 0x00200081,
	0x0060000a, 0x00104280, 0x00200316, 0x0060000a, 0x00200004, 0x00800001,
	0x00700000, 0x00200000, 0x0060000a, 0x00106002, 0x0040a068, 0x00700000,
	0x00200000, 0x0060000a, 0x00106002, 0x00700080, 0x00400a68, 0x00500060,
	0x00600007, 0x00409388, 0x0060000f, 0x00500060, 0x00200000, 0x0060000a,
	0x00700000, 0x00106001, 0x00910880, 0x00901ffe, 0x01940000, 0x00200020,
	0x0060000b, 0x00500069, 0x0060000c, 0x00402168, 0x0040a206, 0x0040a305,
	0x00600009, 0x00700005, 0x00700006, 0x0060000e, ~0
};

static uint32_t nv4a_ctx_voodoo[] = {
	0x00400889, 0x00200000, 0x0060000a, 0x00200000, 0x00300000, 0x00800001, 
	0x00700009, 0x0060000e, 0x00400d64, 0x00400d05, 0x00409965, 0x00409e06, 
	0x0040ac68, 0x00200000, 0x0060000a, 0x00700000, 0x00106000, 0x00700080, 
	0x004014e6, 0x007000a0, 0x00401a84, 0x00700082, 0x00600001, 0x00500061, 
	0x00600002, 0x00401b68, 0x00500060, 0x00200001, 0x0060000a, 0x0011814d, 
	0x00110158, 0x00105401, 0x0020003a, 0x00100051, 0x001040c5, 0x0010c1c4, 
	0x001041c9, 0x0010c1dc, 0x00150210, 0x0012c225, 0x00108238, 0x0010823e, 
	0x001242c0, 0x00200040, 0x00100280, 0x00128100, 0x00128120, 0x00128143, 
	0x0011415f, 0x0010815c, 0x0010c140, 0x00104029, 0x00110400, 0x00104d10, 
	0x001046ec, 0x00500060, 0x00403a87, 0x0060000d, 0x00407de6, 0x002000f1, 
	0x0060000a, 0x00148653, 0x00104668, 0x0010c66d, 0x00120682, 0x0011068b, 
	0x00168691, 0x001046ae, 0x001046b0, 0x001206b4, 0x001046c4, 0x001146c6, 
	0x001646cc, 0x001186e6, 0x001046ed, 0x001246f0, 0x002000c0, 0x00100700, 
	0x0010c3d7, 0x001043e1, 0x00500060, 0x00405800, 0x00405884, 0x00600003, 
	0x00500067, 0x00600008, 0x00500060, 0x00700082, 0x00200232, 0x0060000a, 
	0x00104800, 0x00108901, 0x00104910, 0x00124920, 0x0020001f, 0x00100940, 
	0x00140965, 0x00148a00, 0x00108a14, 0x00160b00, 0x00134b2c, 0x0010cd00, 
	0x0010cd04, 0x0010cd08, 0x00104d80, 0x00104e00, 0x0012d600, 0x00105c00, 
	0x00104f06, 0x002002c8, 0x0060000a, 0x00300000, 0x00200080, 0x00407300, 
	0x00200084, 0x00800001, 0x00200510, 0x0060000a, 0x002037e0, 0x0040798a, 
	0x00201320, 0x00800029, 0x00407d84, 0x00201560, 0x00800002, 0x00409100, 
	0x00600006, 0x00700003, 0x00408ae6, 0x00700080, 0x0020007a, 0x0060000a, 
	0x00104280, 0x002002c8, 0x0060000a, 0x00200004, 0x00800001, 0x00700000, 
	0x00200000, 0x0060000a, 0x00106002, 0x0040ac84, 0x00700002, 0x00600004, 
	0x0040ac68, 0x00700000, 0x00200000, 0x0060000a, 0x00106002, 0x00700080, 
	0x00400a84, 0x00700002, 0x00400a68, 0x00500060, 0x00600007, 0x00409d88, 
	0x0060000f, 0x00000000, 0x00500060, 0x00200000, 0x0060000a, 0x00700000, 
	0x00106001, 0x00700083, 0x00910880, 0x00901ffe, 0x01940000, 0x00200020, 
	0x0060000b, 0x00500069, 0x0060000c, 0x00401b68, 0x0040ae06, 0x0040af05, 
	0x00600009, 0x00700005, 0x00700006, 0x0060000e, ~0
};

static uint32_t nv4e_ctx_voodoo[] = {
	0x00400889, 0x00200000, 0x0060000a, 0x00200000, 0x00300000, 0x00800001,
	0x00700009, 0x0060000e, 0x00400d64, 0x00400d05, 0x00409565, 0x00409a06,
	0x0040a868, 0x00200000, 0x0060000a, 0x00700000, 0x00106000, 0x00700080,
	0x004014e6, 0x007000a0, 0x00401a84, 0x00700082, 0x00600001, 0x00500061,
	0x00600002, 0x00401b68, 0x00500060, 0x00200001, 0x0060000a, 0x0011814d,
	0x00110158, 0x00105401, 0x0020003a, 0x00100051, 0x001040c5, 0x0010c1c4,
	0x001041c9, 0x0010c1dc, 0x00150210, 0x0012c225, 0x00108238, 0x0010823e,
	0x001242c0, 0x00200040, 0x00100280, 0x00128100, 0x00128120, 0x00128143,
	0x0011415f, 0x0010815c, 0x0010c140, 0x00104029, 0x00110400, 0x00104d10,
	0x001046ec, 0x00500060, 0x00403a87, 0x0060000d, 0x00407ce6, 0x002000f1,
	0x0060000a, 0x00148653, 0x00104668, 0x0010c66d, 0x00120682, 0x0011068b,
	0x00168691, 0x001046ae, 0x001046b0, 0x001206b4, 0x001046c4, 0x001146c6,
	0x001646cc, 0x001186e6, 0x001046ed, 0x001246f0, 0x002000c0, 0x00100700,
	0x0010c3d7, 0x001043e1, 0x00500060, 0x00405800, 0x00405884, 0x00600003,
	0x00500067, 0x00600008, 0x00500060, 0x00700082, 0x00200232, 0x0060000a,
	0x00104800, 0x00108901, 0x00104910, 0x00124920, 0x0020001f, 0x00100940,
	0x00140965, 0x00148a00, 0x00108a14, 0x00140b00, 0x00134b2c, 0x0010cd00,
	0x0010cd04, 0x00104d08, 0x00104d80, 0x00104e00, 0x00105c00, 0x00104f06,
	0x002002b2, 0x0060000a, 0x00300000, 0x00200080, 0x00407200, 0x00200084,
	0x00800001, 0x002004fa, 0x0060000a, 0x00201320, 0x0040788a, 0xfffffb06,
	0x00800029, 0x00407c84, 0x00200b20, 0x00800002, 0x00408d00, 0x00600006,
	0x00700003, 0x004086e6, 0x00700080, 0x002002b2, 0x0060000a, 0x00200004,
	0x00800001, 0x00700000, 0x00200000, 0x0060000a, 0x00106002, 0x0040a884,
	0x00700002, 0x00600004, 0x0040a868, 0x00700000, 0x00200000, 0x0060000a,
	0x00106002, 0x00700080, 0x00400a84, 0x00700002, 0x00400a68, 0x00500060,
	0x00600007, 0x00409988, 0x0060000f, 0x00000000, 0x00500060, 0x00200000,
	0x0060000a, 0x00700000, 0x00106001, 0x00700083, 0x00910880, 0x00901ffe,
	0x01940000, 0x00200020, 0x0060000b, 0x00500069, 0x0060000c, 0x00401b68,
	0x0040aa06, 0x0040ab05, 0x00600009, 0x00700005, 0x00700006, 0x0060000e,
	~0
};

/*
 * G70		0x47
 * G71		0x49
 * NV45		0x48
 * G72[M]	0x46
 * G73		0x4b
 * C51_G7X	0x4c
 * C51		0x4e
 */
int
nv40_graph_init(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv =
		(drm_nouveau_private_t *)dev->dev_private;
	uint32_t *ctx_voodoo;
	uint32_t vramsz, tmp;
	int i, j;

	NV_WRITE(NV03_PMC_ENABLE, NV_READ(NV03_PMC_ENABLE) &
			~NV_PMC_ENABLE_PGRAPH);
	NV_WRITE(NV03_PMC_ENABLE, NV_READ(NV03_PMC_ENABLE) |
			 NV_PMC_ENABLE_PGRAPH);

	switch (dev_priv->chipset) {
	case 0x40: ctx_voodoo = nv40_ctx_voodoo; break;
	case 0x43: ctx_voodoo = nv43_ctx_voodoo; break;
	case 0x46: ctx_voodoo = nv46_ctx_voodoo; break;
	case 0x4a: ctx_voodoo = nv4a_ctx_voodoo; break;
	case 0x4e: ctx_voodoo = nv4e_ctx_voodoo; break;
	default:
		DRM_ERROR("Unknown ctx_voodoo for chipset 0x%02x\n",
				dev_priv->chipset);
		ctx_voodoo = NULL;
		break;
	}

	/* Load the context voodoo onto the card */
	if (ctx_voodoo) {
		DRM_DEBUG("Loading context-switch voodoo\n");
		i = 0;

		NV_WRITE(0x400324, 0);
		while (ctx_voodoo[i] != ~0) {
			NV_WRITE(0x400328, ctx_voodoo[i]);
			i++;
		}
	}	

	/* No context present currently */
	NV_WRITE(0x40032C, 0x00000000);

	NV_WRITE(NV03_PGRAPH_INTR_EN, 0x00000000);
	NV_WRITE(NV03_PGRAPH_INTR   , 0xFFFFFFFF);

	NV_WRITE(NV04_PGRAPH_DEBUG_0, 0xFFFFFFFF);
	NV_WRITE(NV04_PGRAPH_DEBUG_0, 0x00000000);
	NV_WRITE(NV04_PGRAPH_DEBUG_1, 0x401287c0);
	NV_WRITE(NV04_PGRAPH_DEBUG_3, 0xe0de8055);
	NV_WRITE(NV10_PGRAPH_DEBUG_4, 0x00008000);
	NV_WRITE(NV04_PGRAPH_LIMIT_VIOL_PIX, 0x00be3c5f);

	NV_WRITE(NV10_PGRAPH_CTX_CONTROL, 0x10010100);
	NV_WRITE(NV10_PGRAPH_STATE      , 0xFFFFFFFF);
	NV_WRITE(NV04_PGRAPH_FIFO       , 0x00000001);

	j = NV_READ(0x1540) & 0xff;
	if (j) {
		for (i=0; !(j&1); j>>=1, i++);
		NV_WRITE(0x405000, i);
	}

	if (dev_priv->chipset == 0x40) {
		NV_WRITE(0x4009b0, 0x83280fff);
		NV_WRITE(0x4009b4, 0x000000a0);
	} else {
		NV_WRITE(0x400820, 0x83280eff);
		NV_WRITE(0x400824, 0x000000a0);
	}

	switch (dev_priv->chipset) {
	case 0x40:
	case 0x45:
		NV_WRITE(0x4009b8, 0x0078e366);
		NV_WRITE(0x4009bc, 0x0000014c);
		break;
	case 0x41:
	case 0x42: /* pciid also 0x00Cx */
//	case 0x0120: //XXX (pciid)
		NV_WRITE(0x400828, 0x007596ff);
		NV_WRITE(0x40082c, 0x00000108);
		break;
	case 0x43:
		NV_WRITE(0x400828, 0x0072cb77);
		NV_WRITE(0x40082c, 0x00000108);
		break;
	case 0x44:
	case 0x46: /* G72 */
	case 0x4a:
	case 0x4c: /* G7x-based C51 */
	case 0x4e:
		NV_WRITE(0x400860, 0);
		NV_WRITE(0x400864, 0);
		break;
	case 0x47: /* G70 */
	case 0x49: /* G71 */
	case 0x4b: /* G73 */
		NV_WRITE(0x400828, 0x07830610);
		NV_WRITE(0x40082c, 0x0000016A);
		break;
	default:
		break;
	}

	NV_WRITE(0x400b38, 0x2ffff800);
	NV_WRITE(0x400b3c, 0x00006000);

	/* copy tile info from PFB */
	switch (dev_priv->chipset) {
	case 0x40: /* vanilla NV40 */
		for (i=0; i<NV10_PFB_TILE__SIZE; i++) {
			tmp = NV_READ(NV10_PFB_TILE(i));
			NV_WRITE(NV40_PGRAPH_TILE0(i), tmp);
			NV_WRITE(NV40_PGRAPH_TILE1(i), tmp);
			tmp = NV_READ(NV10_PFB_TLIMIT(i));
			NV_WRITE(NV40_PGRAPH_TLIMIT0(i), tmp);
			NV_WRITE(NV40_PGRAPH_TLIMIT1(i), tmp);
			tmp = NV_READ(NV10_PFB_TSIZE(i));
			NV_WRITE(NV40_PGRAPH_TSIZE0(i), tmp);
			NV_WRITE(NV40_PGRAPH_TSIZE1(i), tmp);
			tmp = NV_READ(NV10_PFB_TSTATUS(i));
			NV_WRITE(NV40_PGRAPH_TSTATUS0(i), tmp);
			NV_WRITE(NV40_PGRAPH_TSTATUS1(i), tmp);
		}
		break;
	case 0x44:
	case 0x4a:
	case 0x4e: /* NV44-based cores don't have 0x406900? */
		for (i=0; i<NV40_PFB_TILE__SIZE_0; i++) {
			tmp = NV_READ(NV40_PFB_TILE(i));
			NV_WRITE(NV40_PGRAPH_TILE0(i), tmp);
			tmp = NV_READ(NV40_PFB_TLIMIT(i));
			NV_WRITE(NV40_PGRAPH_TLIMIT0(i), tmp);
			tmp = NV_READ(NV40_PFB_TSIZE(i));
			NV_WRITE(NV40_PGRAPH_TSIZE0(i), tmp);
			tmp = NV_READ(NV40_PFB_TSTATUS(i));
			NV_WRITE(NV40_PGRAPH_TSTATUS0(i), tmp);
		}
		break;
	case 0x46:
	case 0x47:
	case 0x49:
	case 0x4b: /* G7X-based cores */
		for (i=0; i<NV40_PFB_TILE__SIZE_1; i++) {
			tmp = NV_READ(NV40_PFB_TILE(i));
			NV_WRITE(NV47_PGRAPH_TILE0(i), tmp);
			NV_WRITE(NV40_PGRAPH_TILE1(i), tmp);
			tmp = NV_READ(NV40_PFB_TLIMIT(i));
			NV_WRITE(NV47_PGRAPH_TLIMIT0(i), tmp);
			NV_WRITE(NV40_PGRAPH_TLIMIT1(i), tmp);
			tmp = NV_READ(NV40_PFB_TSIZE(i));
			NV_WRITE(NV47_PGRAPH_TSIZE0(i), tmp);
			NV_WRITE(NV40_PGRAPH_TSIZE1(i), tmp);
			tmp = NV_READ(NV40_PFB_TSTATUS(i));
			NV_WRITE(NV47_PGRAPH_TSTATUS0(i), tmp);
			NV_WRITE(NV40_PGRAPH_TSTATUS1(i), tmp);
		}
		break;
	default: /* everything else */
		for (i=0; i<NV40_PFB_TILE__SIZE_0; i++) {
			tmp = NV_READ(NV40_PFB_TILE(i));
			NV_WRITE(NV40_PGRAPH_TILE0(i), tmp);
			NV_WRITE(NV40_PGRAPH_TILE1(i), tmp);
			tmp = NV_READ(NV40_PFB_TLIMIT(i));
			NV_WRITE(NV40_PGRAPH_TLIMIT0(i), tmp);
			NV_WRITE(NV40_PGRAPH_TLIMIT1(i), tmp);
			tmp = NV_READ(NV40_PFB_TSIZE(i));
			NV_WRITE(NV40_PGRAPH_TSIZE0(i), tmp);
			NV_WRITE(NV40_PGRAPH_TSIZE1(i), tmp);
			tmp = NV_READ(NV40_PFB_TSTATUS(i));
			NV_WRITE(NV40_PGRAPH_TSTATUS0(i), tmp);
			NV_WRITE(NV40_PGRAPH_TSTATUS1(i), tmp);
		}
		break;
	}

	/* begin RAM config */
	vramsz = drm_get_resource_len(dev, 0) - 1;
	switch (dev_priv->chipset) {
	case 0x40:
		NV_WRITE(0x4009A4, NV_READ(NV04_PFB_CFG0));
		NV_WRITE(0x4009A8, NV_READ(NV04_PFB_CFG1));
		NV_WRITE(0x4069A4, NV_READ(NV04_PFB_CFG0));
		NV_WRITE(0x4069A8, NV_READ(NV04_PFB_CFG1));
		NV_WRITE(0x400820, 0);
		NV_WRITE(0x400824, 0);
		NV_WRITE(0x400864, vramsz);
		NV_WRITE(0x400868, vramsz);
		break;
	default:
		switch (dev_priv->chipset) {
		case 0x46:
		case 0x47:
		case 0x49:
		case 0x4b:
			NV_WRITE(0x400DF0, NV_READ(NV04_PFB_CFG0));
			NV_WRITE(0x400DF4, NV_READ(NV04_PFB_CFG1));
			break;
		default:
			NV_WRITE(0x4009F0, NV_READ(NV04_PFB_CFG0));
			NV_WRITE(0x4009F4, NV_READ(NV04_PFB_CFG1));
			break;
		}
		NV_WRITE(0x4069F0, NV_READ(NV04_PFB_CFG0));
		NV_WRITE(0x4069F4, NV_READ(NV04_PFB_CFG1));
		NV_WRITE(0x400840, 0);
		NV_WRITE(0x400844, 0);
		NV_WRITE(0x4008A0, vramsz);
		NV_WRITE(0x4008A4, vramsz);
		break;
	}

	/* per-context state, doesn't belong here */
	NV_WRITE(0x400B20, 0x00000000);
	NV_WRITE(0x400B04, 0xFFFFFFFF);

	tmp = NV_READ(NV10_PGRAPH_SURFACE) & 0x0007ff00;
	NV_WRITE(NV10_PGRAPH_SURFACE, tmp);
	tmp = NV_READ(NV10_PGRAPH_SURFACE) | 0x00020100;
	NV_WRITE(NV10_PGRAPH_SURFACE, tmp);

	NV_WRITE(NV03_PGRAPH_ABS_UCLIP_XMIN, 0);
	NV_WRITE(NV03_PGRAPH_ABS_UCLIP_YMIN, 0);
	NV_WRITE(NV03_PGRAPH_ABS_UCLIP_XMAX, 0x7fff);
	NV_WRITE(NV03_PGRAPH_ABS_UCLIP_YMAX, 0x7fff);

	return 0;
}

void nv40_graph_takedown(drm_device_t *dev)
{
}

