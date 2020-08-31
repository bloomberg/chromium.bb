// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_param_traits.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "base/unguessable_token.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "content/common/content_to_visible_time_reporter.h"
#include "ipc/ipc_mojo_message_helper.h"
#include "ipc/ipc_mojo_param_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/cpp/net_ipc_param_traits.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/mojom/feature_policy/policy_value.mojom.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

namespace IPC {

void ParamTraits<content::WebCursor>::Write(base::Pickle* m,
                                            const param_type& p) {
  WriteParam(m, p.cursor().type());
  if (p.cursor().type() == ui::mojom::CursorType::kCustom) {
    WriteParam(m, p.cursor().custom_hotspot());
    WriteParam(m, p.cursor().image_scale_factor());
    WriteParam(m, p.cursor().custom_bitmap());
  }
}

bool ParamTraits<content::WebCursor>::Read(const base::Pickle* m,
                                           base::PickleIterator* iter,
                                           param_type* r) {
  ui::mojom::CursorType type;
  if (!ReadParam(m, iter, &type))
    return false;

  ui::Cursor cursor(type);
  if (cursor.type() == ui::mojom::CursorType::kCustom) {
    gfx::Point hotspot;
    float image_scale_factor;
    SkBitmap bitmap;
    if (!ReadParam(m, iter, &hotspot) ||
        !ReadParam(m, iter, &image_scale_factor) ||
        !ReadParam(m, iter, &bitmap)) {
      return false;
    }

    cursor.set_custom_hotspot(hotspot);
    cursor.set_image_scale_factor(image_scale_factor);
    cursor.set_custom_bitmap(bitmap);
  }

  return r->SetCursor(cursor);
}

void ParamTraits<content::WebCursor>::Log(const param_type& p, std::string* l) {
  l->append("<WebCursor>");
}

void ParamTraits<blink::MessagePortChannel>::Write(base::Pickle* m,
                                                   const param_type& p) {
  ParamTraits<blink::MessagePortDescriptor>::Write(m, p.ReleaseHandle());
}

bool ParamTraits<blink::MessagePortChannel>::Read(const base::Pickle* m,
                                                  base::PickleIterator* iter,
                                                  param_type* r) {
  blink::MessagePortDescriptor port;
  if (!ParamTraits<blink::MessagePortDescriptor>::Read(m, iter, &port))
    return false;
  *r = blink::MessagePortChannel(std::move(port));
  return true;
}

void ParamTraits<blink::MessagePortChannel>::Log(const param_type& p,
                                                 std::string* l) {}

void ParamTraits<blink::PolicyValue>::Write(base::Pickle* m,
                                            const param_type& p) {
  blink::mojom::PolicyValueType type = p.Type();
  WriteParam(m, static_cast<int>(type));
  switch (type) {
    case blink::mojom::PolicyValueType::kBool:
      WriteParam(m, p.BoolValue());
      break;
    case blink::mojom::PolicyValueType::kDecDouble:
      WriteParam(m, p.DoubleValue());
      break;
    case blink::mojom::PolicyValueType::kNull:
      break;
  }
}

void ParamTraits<blink::MessagePortDescriptor>::Write(
    base::Pickle* m,
    const param_type& pconst) {
  // Serializing this object is a move of the object contents, thus we need a
  // mutable reference to it.
  param_type& p = const_cast<param_type&>(pconst);
  ParamTraits<mojo::MessagePipeHandle>::Write(
      m, p.TakeHandleForSerialization().release());
  ParamTraits<base::UnguessableToken>::Write(m, p.TakeIdForSerialization());
  ParamTraits<uint64_t>::Write(m, p.TakeSequenceNumberForSerialization());
}

bool ParamTraits<blink::MessagePortDescriptor>::Read(const base::Pickle* m,
                                                     base::PickleIterator* iter,
                                                     param_type* r) {
  mojo::MessagePipeHandle port;
  base::UnguessableToken id;
  uint64_t sequence_number = 0;
  if (!ParamTraits<mojo::MessagePipeHandle>::Read(m, iter, &port) ||
      !ParamTraits<base::UnguessableToken>::Read(m, iter, &id) ||
      !ParamTraits<uint64_t>::Read(m, iter, &sequence_number)) {
    return false;
  }
  r->InitializeFromSerializedValues(mojo::ScopedMessagePipeHandle(port), id,
                                    sequence_number);
  return true;
}

void ParamTraits<blink::MessagePortDescriptor>::Log(const param_type& p,
                                                    std::string* l) {}

bool ParamTraits<blink::PolicyValue>::Read(const base::Pickle* m,
                                           base::PickleIterator* iter,
                                           param_type* r) {
  int int_type;
  if (!ReadParam(m, iter, &int_type))
    return false;
  blink::mojom::PolicyValueType type =
      static_cast<blink::mojom::PolicyValueType>(int_type);
  r->SetType(type);
  switch (type) {
    case blink::mojom::PolicyValueType::kBool: {
      bool b;
      if (!ReadParam(m, iter, &b))
        return false;
      r->SetBoolValue(b);
      break;
    }
    case blink::mojom::PolicyValueType::kDecDouble: {
      double d;
      if (!ReadParam(m, iter, &d))
        return false;
      r->SetDoubleValue(d, type);
      break;
    }
    case blink::mojom::PolicyValueType::kNull:
      break;
  }
  return true;
}

void ParamTraits<blink::PolicyValue>::Log(const param_type& p, std::string* l) {
}

void ParamTraits<ui::AXMode>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.mode());
}

bool ParamTraits<ui::AXMode>::Read(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   param_type* r) {
  uint32_t value;
  if (!ReadParam(m, iter, &value))
    return false;
  *r = ui::AXMode(value);
  return true;
}

void ParamTraits<ui::AXMode>::Log(const param_type& p, std::string* l) {}

template <>
struct ParamTraits<blink::mojom::SerializedBlobPtr> {
  using param_type = blink::mojom::SerializedBlobPtr;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p->uuid);
    WriteParam(m, p->content_type);
    WriteParam(m, p->size);
    WriteParam(m, p->blob.PassPipe().release());
  }

  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    *r = blink::mojom::SerializedBlob::New();
    mojo::MessagePipeHandle handle;
    if (!ReadParam(m, iter, &(*r)->uuid) ||
        !ReadParam(m, iter, &(*r)->content_type) ||
        !ReadParam(m, iter, &(*r)->size) || !ReadParam(m, iter, &handle)) {
      return false;
    }
    (*r)->blob = mojo::PendingRemote<blink::mojom::Blob>(
        mojo::ScopedMessagePipeHandle(handle), blink::mojom::Blob::Version_);
    return true;
  }
};

template <>
struct ParamTraits<
    mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken>> {
  using param_type =
      mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken>;
  static void Write(base::Pickle* m, const param_type& p) {
    // Move the Mojo pipe to serialize the
    // PendingRemote<NativeFileSystemTransferToken> for a postMessage() target.
    WriteParam(m, const_cast<param_type&>(p).PassPipe().release());
  }

  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    mojo::MessagePipeHandle handle;
    if (!ReadParam(m, iter, &handle)) {
      return false;
    }
    *r = mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken>(
        mojo::ScopedMessagePipeHandle(handle),
        blink::mojom::NativeFileSystemTransferToken::Version_);
    return true;
  }
};

void ParamTraits<scoped_refptr<base::RefCountedData<
    blink::TransferableMessage>>>::Write(base::Pickle* m, const param_type& p) {
  m->WriteData(reinterpret_cast<const char*>(p->data.encoded_message.data()),
               p->data.encoded_message.size());
  WriteParam(m, p->data.blobs);
  WriteParam(m, p->data.stack_trace_id);
  WriteParam(m, p->data.stack_trace_debugger_id_first);
  WriteParam(m, p->data.stack_trace_debugger_id_second);
  WriteParam(m, p->data.stack_trace_should_pause);
  WriteParam(m, p->data.locked_agent_cluster_id);
  WriteParam(m, p->data.ports);
  WriteParam(m, p->data.stream_channels);
  WriteParam(m, !!p->data.user_activation);
  WriteParam(m, p->data.transfer_user_activation);
  WriteParam(m, p->data.allow_autoplay);
  WriteParam(m, p->data.sender_origin);
  WriteParam(m, p->data.native_file_system_tokens);
  if (p->data.user_activation) {
    WriteParam(m, p->data.user_activation->has_been_active);
    WriteParam(m, p->data.user_activation->was_active);
  }
}

bool ParamTraits<
    scoped_refptr<base::RefCountedData<blink::TransferableMessage>>>::
    Read(const base::Pickle* m, base::PickleIterator* iter, param_type* r) {
  *r = new base::RefCountedData<blink::TransferableMessage>();

  const char* data;
  int length;
  if (!iter->ReadData(&data, &length))
    return false;
  // This just makes encoded_message point into the IPC message buffer. Usually
  // code receiving a TransferableMessage will synchronously process the message
  // so this avoids an unnecessary copy. If a receiver needs to hold on to the
  // message longer, it should make sure to call EnsureDataIsOwned on the
  // returned message.
  (*r)->data.encoded_message =
      base::make_span(reinterpret_cast<const uint8_t*>(data), length);
  bool has_activation = false;
  if (!ReadParam(m, iter, &(*r)->data.blobs) ||
      !ReadParam(m, iter, &(*r)->data.stack_trace_id) ||
      !ReadParam(m, iter, &(*r)->data.stack_trace_debugger_id_first) ||
      !ReadParam(m, iter, &(*r)->data.stack_trace_debugger_id_second) ||
      !ReadParam(m, iter, &(*r)->data.stack_trace_should_pause) ||
      !ReadParam(m, iter, &(*r)->data.locked_agent_cluster_id) ||
      !ReadParam(m, iter, &(*r)->data.ports) ||
      !ReadParam(m, iter, &(*r)->data.stream_channels) ||
      !ReadParam(m, iter, &has_activation) ||
      !ReadParam(m, iter, &(*r)->data.transfer_user_activation) ||
      !ReadParam(m, iter, &(*r)->data.allow_autoplay) ||
      !ReadParam(m, iter, &(*r)->data.sender_origin) ||
      !ReadParam(m, iter, &(*r)->data.native_file_system_tokens)) {
    return false;
  }

  if (has_activation) {
    bool has_been_active;
    bool was_active;
    if (!ReadParam(m, iter, &has_been_active) ||
        !ReadParam(m, iter, &was_active)) {
      return false;
    }
    (*r)->data.user_activation =
        blink::mojom::UserActivationSnapshot::New(has_been_active, was_active);
  }
  return true;
}

void ParamTraits<scoped_refptr<
    base::RefCountedData<blink::TransferableMessage>>>::Log(const param_type& p,
                                                            std::string* l) {
  l->append("<blink::TransferableMessage>");
}

void ParamTraits<viz::FrameSinkId>::Write(base::Pickle* m,
                                          const param_type& p) {
  DCHECK(p.is_valid());
  WriteParam(m, p.client_id());
  WriteParam(m, p.sink_id());
}

bool ParamTraits<viz::FrameSinkId>::Read(const base::Pickle* m,
                                         base::PickleIterator* iter,
                                         param_type* p) {
  uint32_t client_id;
  if (!ReadParam(m, iter, &client_id))
    return false;

  uint32_t sink_id;
  if (!ReadParam(m, iter, &sink_id))
    return false;

  *p = viz::FrameSinkId(client_id, sink_id);
  return p->is_valid();
}

void ParamTraits<viz::FrameSinkId>::Log(const param_type& p, std::string* l) {
  l->append("viz::FrameSinkId(");
  LogParam(p.client_id(), l);
  l->append(", ");
  LogParam(p.sink_id(), l);
  l->append(")");
}

void ParamTraits<viz::LocalSurfaceId>::Write(base::Pickle* m,
                                             const param_type& p) {
  DCHECK(p.is_valid());
  WriteParam(m, p.parent_sequence_number());
  WriteParam(m, p.child_sequence_number());
  WriteParam(m, p.embed_token());
}

bool ParamTraits<viz::LocalSurfaceId>::Read(const base::Pickle* m,
                                            base::PickleIterator* iter,
                                            param_type* p) {
  uint32_t parent_sequence_number;
  if (!ReadParam(m, iter, &parent_sequence_number))
    return false;

  uint32_t child_sequence_number;
  if (!ReadParam(m, iter, &child_sequence_number))
    return false;

  base::UnguessableToken embed_token;
  if (!ReadParam(m, iter, &embed_token))
    return false;

  *p = viz::LocalSurfaceId(parent_sequence_number, child_sequence_number,
                           embed_token);
  return p->is_valid();
}

void ParamTraits<viz::LocalSurfaceId>::Log(const param_type& p,
                                           std::string* l) {
  l->append("viz::LocalSurfaceId(");
  LogParam(p.parent_sequence_number(), l);
  l->append(", ");
  LogParam(p.child_sequence_number(), l);
  l->append(", ");
  LogParam(p.embed_token(), l);
  l->append(")");
}

void ParamTraits<viz::LocalSurfaceIdAllocation>::Write(base::Pickle* m,
                                                       const param_type& p) {
  DCHECK(p.IsValid());
  WriteParam(m, p.local_surface_id());
  WriteParam(m, p.allocation_time());
}

bool ParamTraits<viz::LocalSurfaceIdAllocation>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  viz::LocalSurfaceId local_surface_id;
  if (!ReadParam(m, iter, &local_surface_id))
    return false;

  base::TimeTicks allocation_time;
  if (!ReadParam(m, iter, &allocation_time))
    return false;

  *p = viz::LocalSurfaceIdAllocation(local_surface_id, allocation_time);
  return p->IsValid();
}

void ParamTraits<viz::LocalSurfaceIdAllocation>::Log(const param_type& p,
                                                     std::string* l) {
  l->append("viz::LocalSurfaceIdAllocation(");
  LogParam(p.local_surface_id(), l);
  l->append(", ");
  LogParam(p.allocation_time(), l);
  l->append(")");
}

void ParamTraits<viz::SurfaceId>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.frame_sink_id());
  WriteParam(m, p.local_surface_id());
}

bool ParamTraits<viz::SurfaceId>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* p) {
  viz::FrameSinkId frame_sink_id;
  if (!ReadParam(m, iter, &frame_sink_id))
    return false;

  viz::LocalSurfaceId local_surface_id;
  if (!ReadParam(m, iter, &local_surface_id))
    return false;

  *p = viz::SurfaceId(frame_sink_id, local_surface_id);
  return true;
}

void ParamTraits<viz::SurfaceId>::Log(const param_type& p, std::string* l) {
  l->append("viz::SurfaceId(");
  LogParam(p.frame_sink_id(), l);
  l->append(", ");
  LogParam(p.local_surface_id(), l);
  l->append(")");
}

void ParamTraits<viz::SurfaceInfo>::Write(base::Pickle* m,
                                          const param_type& p) {
  WriteParam(m, p.id());
  WriteParam(m, p.device_scale_factor());
  WriteParam(m, p.size_in_pixels());
}

bool ParamTraits<viz::SurfaceInfo>::Read(const base::Pickle* m,
                                         base::PickleIterator* iter,
                                         param_type* p) {
  viz::SurfaceId surface_id;
  if (!ReadParam(m, iter, &surface_id))
    return false;

  float device_scale_factor;
  if (!ReadParam(m, iter, &device_scale_factor))
    return false;

  gfx::Size size_in_pixels;
  if (!ReadParam(m, iter, &size_in_pixels))
    return false;

  *p = viz::SurfaceInfo(surface_id, device_scale_factor, size_in_pixels);
  return p->is_valid();
}

void ParamTraits<viz::SurfaceInfo>::Log(const param_type& p, std::string* l) {
  l->append("viz::SurfaceInfo(");
  LogParam(p.id(), l);
  l->append(", ");
  LogParam(p.device_scale_factor(), l);
  l->append(", ");
  LogParam(p.size_in_pixels(), l);
  l->append(")");
}

void ParamTraits<net::SHA256HashValue>::Write(base::Pickle* m,
                                              const param_type& p) {
  m->WriteData(reinterpret_cast<const char*>(p.data), sizeof(p.data));
}

bool ParamTraits<net::SHA256HashValue>::Read(const base::Pickle* m,
                                             base::PickleIterator* iter,
                                             param_type* r) {
  const char* data;
  int data_length;
  if (!iter->ReadData(&data, &data_length)) {
    NOTREACHED();
    return false;
  }
  if (data_length != sizeof(r->data)) {
    NOTREACHED();
    return false;
  }
  memcpy(r->data, data, sizeof(r->data));
  return true;
}

void ParamTraits<net::SHA256HashValue>::Log(const param_type& p,
                                            std::string* l) {
  l->append("<SHA256HashValue>");
}

void ParamTraits<content::RecordContentToVisibleTimeRequest>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p.event_start_time);
  WriteParam(m, p.destination_is_loaded);
  WriteParam(m, p.destination_is_frozen);
  WriteParam(m, p.show_reason_tab_switching);
  WriteParam(m, p.show_reason_unoccluded);
  WriteParam(m, p.show_reason_bfcache_restore);
}

bool ParamTraits<content::RecordContentToVisibleTimeRequest>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  if (!ReadParam(m, iter, &r->event_start_time) ||
      !ReadParam(m, iter, &r->destination_is_loaded) ||
      !ReadParam(m, iter, &r->destination_is_frozen) ||
      !ReadParam(m, iter, &r->show_reason_tab_switching) ||
      !ReadParam(m, iter, &r->show_reason_unoccluded) ||
      !ReadParam(m, iter, &r->show_reason_bfcache_restore)) {
    return false;
  }

  return true;
}

void ParamTraits<content::RecordContentToVisibleTimeRequest>::Log(
    const param_type& p,
    std::string* l) {
  l->append("<content::RecordContentToVisibleTimeRequest>");
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#include "content/common/content_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#include "content/common/content_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#include "content/common/content_param_traits_macros.h"
}  // namespace IPC
