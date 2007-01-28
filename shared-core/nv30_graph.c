/*
 * Based on nv40_graph.c
 *  Someday this will all go away...
 */
#include "drmP.h"
#include "drm.h"
#include "nouveau_drv.h"
#include "nouveau_drm.h"

/*
 *  This is obviously not the correct size. 
 */
#define NV30_GRCTX_SIZE (23840)

/*TODO: deciper what each offset in the context represents. The below
 *      contexts are taken from dumps just after the 3D object is
 *      created.
 */
static void nv30_graph_context_init(drm_device_t *dev, struct mem_block *ctx)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int i;
        
        INSTANCE_WR(ctx, 0x28/4,  0x10000000);
        INSTANCE_WR(ctx, 0x40c/4, 0x00000101);
        INSTANCE_WR(ctx, 0x420/4, 0x00000111);
        INSTANCE_WR(ctx, 0x424/4, 0x00000060);
        INSTANCE_WR(ctx, 0x440/4, 0x00000080);
        INSTANCE_WR(ctx, 0x444/4, 0xffff0000);
        INSTANCE_WR(ctx, 0x448/4, 0x00000001);
        INSTANCE_WR(ctx, 0x45c/4, 0x44400000);
        INSTANCE_WR(ctx, 0x448/4, 0xffff0000);
        INSTANCE_WR(ctx, 0x4dc/4, 0xfff00000);
        INSTANCE_WR(ctx, 0x4e0/4, 0xfff00000);
        INSTANCE_WR(ctx, 0x4e8/4, 0x00011100);

        for (i = 0x504; i <= 0x540; i += 4)
                INSTANCE_WR(ctx, i/4, 0x7ff00000);

        INSTANCE_WR(ctx, 0x54c/4, 0x4b7fffff);
        INSTANCE_WR(ctx, 0x588/4, 0x00000080);
        INSTANCE_WR(ctx, 0x58c/4, 0x30201000);
        INSTANCE_WR(ctx, 0x590/4, 0x70605040);
        INSTANCE_WR(ctx, 0x594/4, 0xb8a89888);
        INSTANCE_WR(ctx, 0x598/4, 0xf8e8d8c8);
        INSTANCE_WR(ctx, 0x5ac/4, 0xb0000000);

        for (i = 0x604; i <= 0x640; i += 4)
                INSTANCE_WR(ctx, i/4, 0x00010588);

        for (i = 0x644; i <= 0x680; i += 4)
                INSTANCE_WR(ctx, i/4, 0x00030303);

        for (i = 0x6c4; i <= 0x700; i += 4)
                INSTANCE_WR(ctx, i/4, 0x0008aae4);

        for (i = 0x704; i <= 0x740; i += 4)
                INSTANCE_WR(ctx, i/4, 0x1012000);

        for (i = 0x744; i <= 0x780; i += 4)
                INSTANCE_WR(ctx, i/4, 0x0080008);

        INSTANCE_WR(ctx, 0x860/4, 0x00040000);
        INSTANCE_WR(ctx, 0x864/4, 0x00010000);
        INSTANCE_WR(ctx, 0x868/4, 0x00040000);
        INSTANCE_WR(ctx, 0x86c/4, 0x00040000);
        INSTANCE_WR(ctx, 0x870/4, 0x00040000);
        INSTANCE_WR(ctx, 0x874/4, 0x00040000);

        for (i = 0x00; i <= 0x1170; i += 0x10)
        {
                INSTANCE_WR(ctx, (0x1f24 + i)/4, 0x000c001b);
                INSTANCE_WR(ctx, (0x1f20 + i)/4, 0x0436086c);
                INSTANCE_WR(ctx, (0x1f1c + i)/4, 0x10700ff9);
        }

        INSTANCE_WR(ctx, 0x30bc/4, 0x0000ffff);
        INSTANCE_WR(ctx, 0x30c0/4, 0x0000ffff);
        INSTANCE_WR(ctx, 0x30c4/4, 0x0000ffff);
        INSTANCE_WR(ctx, 0x30c8/4, 0x0000ffff);

        INSTANCE_WR(ctx, 0x380c/4, 0x3f800000);
        INSTANCE_WR(ctx, 0x3450/4, 0x3f800000);
        INSTANCE_WR(ctx, 0x3820/4, 0x3f800000);
        INSTANCE_WR(ctx, 0x3854/4, 0x3f800000);
        INSTANCE_WR(ctx, 0x3850/4, 0x3f000000);
        INSTANCE_WR(ctx, 0x384c/4, 0x40000000);
        INSTANCE_WR(ctx, 0x3868/4, 0xbf800000);
        INSTANCE_WR(ctx, 0x3860/4, 0x3f800000);
        INSTANCE_WR(ctx, 0x386c/4, 0x40000000);
        INSTANCE_WR(ctx, 0x3870/4, 0xbf800000);

        for (i = 0x4e0; i <= 0x4e1c; i += 4)
                INSTANCE_WR(ctx, i/4, 0x001c527d);
        INSTANCE_WR(ctx, 0x4e40, 0x001c527c);

        INSTANCE_WR(ctx, 0x5680/4, 0x000a0000);
        INSTANCE_WR(ctx, 0x87c/4, 0x10000000);
        INSTANCE_WR(ctx, 0x28/4, 0x10000011);
}


int nv30_graph_context_create(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv =
		(drm_nouveau_private_t *)dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[channel];
	void (*ctx_init)(drm_device_t *, struct mem_block *);
	unsigned int ctx_size;
	int i;

	switch (dev_priv->chipset) {
	default:
		ctx_size = NV30_GRCTX_SIZE;
		ctx_init = nv30_graph_context_init;
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
        
        INSTANCE_WR(chan->ramin_grctx, 10, channel << 24); /* CTX_USER */
        INSTANCE_WR(dev_priv->ctx_table, channel, nouveau_chip_instance_get(dev, chan->ramin_grctx));

	return 0;
}

int nv30_graph_init(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv =
		(drm_nouveau_private_t *)dev->dev_private;
	int i;

        /* Create Context Pointer Table */
        dev_priv->ctx_table_size = 32 * 4;
        dev_priv->ctx_table = nouveau_instmem_alloc(dev, dev_priv->ctx_table_size, 4);
        if (!dev_priv->ctx_table)
                return DRM_ERR(ENOMEM);

        for (i=0; i< dev_priv->ctx_table_size; i+=4)
                INSTANCE_WR(dev_priv->ctx_table, i/4, 0x00000000);

        NV_WRITE(NV_PGRAPH_CHANNEL_CTX_TABLE, nouveau_chip_instance_get(dev, dev_priv->ctx_table));

	return 0;
}

