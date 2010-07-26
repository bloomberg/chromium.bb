/* packet-spdy.h
 *
 * Copyright 2010, Google Inc.
 * Eric Shienbrood <ers@google.com>
 *
 * $Id$
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __PACKET_SPDY_H__
#define __PACKET_SPDY_H__
#pragma once

#include <epan/packet.h>
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

/*
 * Conversation data - used for assembling multi-data-frame
 * entities and for decompressing request & reply header blocks.
 */
typedef struct _spdy_conv_t {
    z_streamp rqst_decompressor;
    z_streamp rply_decompressor;
    guint32   dictionary_id;
    GArray    *streams;
} spdy_conv_t;

#endif /* __PACKET_SPDY_H__ */
