// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/client.h"

#include <string>
#include <iostream>  // TODO(garykac): Remove this or replace with debug log.

#include "base/logging.h"
#include "media/base/yuv_convert.h"
#include "remoting/client/plugin/chromoting_plugin.h"
#include "remoting/client/plugin/chromotocol.h"
#include "remoting/client/plugin/compression.h"
#include "remoting/client/plugin/decoder.h"
#include "remoting/client/plugin/host_connection.h"

namespace remoting {

ChromotingClient::ChromotingClient(ChromotingPlugin* plugin) {
  plugin_ = plugin;
  host_ = new HostConnection();
  verbose_ = true;
}

ChromotingClient::~ChromotingClient() {
}

void ChromotingClient::hexdump(void* ptr, int buflen) {
  unsigned char* buf = static_cast<unsigned char*>(ptr);
  int i, j;
  for (int i = 0; i < buflen; i += 16) {
    printf("%06x: ", i);
    for (int j = 0; j < 16; j ++)
      if ((i + j) < buflen)
        printf("%02x ", buf[i + j]);
      else
        printf("   ");
    printf(" ");
    for (int j = 0; j < 16; j++)
      if ((i + j) < buflen)
        printf("%c", isprint(buf[i + j]) ? buf[i + j] : '.');
    printf("\n");
  }
}

void ChromotingClient::merge_image(BinaryImageHeader* header, char* data) {
  // Merge this image into the current image.
  // Src bitmap starts at lower left.
  int bytes_per_pixel = 3;
  uint8* src_bitmap = reinterpret_cast<uint8*>(data);
  int src_bytes_per_row = header->width * bytes_per_pixel;

  // Dst bitmap starts at lower left.
  uint8* dst_bitmap = reinterpret_cast<uint8*>(screen_->data.get());
  dst_bitmap += ((header->y * screen_->header.width)
                 + header->x) * bytes_per_pixel;
  int dst_bytes_per_row = screen_->header.width * bytes_per_pixel;

  for (int y = 0; y < header->height; ++y) {
    memcpy(dst_bitmap, src_bitmap, src_bytes_per_row);

    src_bitmap += src_bytes_per_row;
    dst_bitmap += dst_bytes_per_row;
  }
}

// Draw the current image into the given device context.
void ChromotingClient::draw(int width, int height, NPDeviceContext2D* context) {
  if (screen_ != NULL) {
    int max_width = width;
    if (max_width > screen_->header.width) {
      max_width = screen_->header.width;
    }
    int max_height = height;
    if (max_height > screen_->header.height) {
      max_height = screen_->header.height;
    }

    // Src bitmap starts at lower left.
    int bytes_per_pixel = 3;
    int src_bytes_per_row = screen_->header.width * bytes_per_pixel;
    uint8* src_bitmap = reinterpret_cast<uint8*>(screen_->data.get());
    src_bitmap += ((max_height - 1) * src_bytes_per_row);
    uint8* src_row_start = reinterpret_cast<uint8*>(src_bitmap);

    // Dst bitmap (window) starts at upper left.
    uint32* dst_bitmap = static_cast<uint32*>(context->region);
    uint8* dst_row_start = reinterpret_cast<uint8*>(dst_bitmap);
    int dst_bytes_per_row = context->stride;

    for (int y = 0; y < max_height; ++y) {
      for (int x = 0; x < max_width; ++x) {
        uint8 b = (*src_bitmap++ & 0xff);
        uint8 g = (*src_bitmap++ & 0xff);
        uint8 r = (*src_bitmap++ & 0xff);
        uint8 alpha = 0xff;
        *dst_bitmap++ = ((alpha << 24) | (r << 16) | (g << 8) | b);
      }
      src_row_start -= src_bytes_per_row;
      src_bitmap = src_row_start;
      dst_row_start += dst_bytes_per_row;
      dst_bitmap = reinterpret_cast<uint32*>(dst_row_start);
    }
  }
}

bool ChromotingClient::connect_to_host(const std::string ip) {
  if (!host_->connect(ip.c_str())) {
    return false;
  }

  // Process init command.
  InitMessage init_message;
  host_->read_data(reinterpret_cast<char*>(&init_message),
                   sizeof(init_message));
  if (verbose_) {
    std::cout << "Received message " << init_message.message << std::endl;
  }
  if (init_message.message != MessageInit) {
    std::cout << "Expected MessageInit" << std::endl;
    return false;
  }
  if (verbose_) {
    std::cout << "Compression: " << init_message.compression << std::endl;
    std::cout << "Width x height: " << init_message.width << " x "
              << init_message.height << std::endl;
  }

  screen_.reset(new BinaryImage());
  if (!read_image(screen_.get())) {
    return false;
  }

  plugin_->draw();
  return true;
}

void ChromotingClient::print_host_ip_prompt() {
  std::cout << "IP address of host machine: ";
}

// This is called whenever the window changes geometry.
// Currently, we only worry about the first call so we can display our
// login prompt.
void ChromotingClient::set_window() {
  if (!host_->connected()) {
    print_host_ip_prompt();
    std::cout.flush();
  }
}

// Process char input.
void ChromotingClient::handle_char_event(NPPepperEvent* npevent) {
  if (!host_->connected()) {
    handle_login_char(static_cast<char>(npevent->u.character.text[0]));
  }
}

// Process char input before the connection to the host has been made.
// This currently reads the IP address of the host but will eventually
// be changed to read GAIA login credentials.
// Later this will be removed once we have an appropriate web interface for
// discovering hosts.
void ChromotingClient::handle_login_char(char ch) {
  if (ch == 0x0d) {
    std::cout << std::endl;
    if (host_ip_address_.length() == 0) {
      host_ip_address_ = "172.31.11.205";
    }
    if (!connect_to_host(host_ip_address_)) {
      host_ip_address_ = "";
      std::cout << "Unable to connect to host!" << std::endl;
      print_host_ip_prompt();
    } else {
      std::cout << "Connected to " << host_ip_address_ << std::endl;
    }
  } else {
    host_ip_address_ += ch;
    std::cout << ch;
  }
  std::cout.flush();
}

// Process the Pepper mouse event.
void ChromotingClient::handle_mouse_event(NPPepperEvent* npevent) {
  if (host_->connected()) {
    send_mouse_message(npevent);
    if (handle_update_message()) {
      plugin_->draw();
    }
  }
}

// Pass the given Pepper mouse event along to the host.
void ChromotingClient::send_mouse_message(NPPepperEvent* event) {
  NPMouseEvent* mouse_event = &event->u.mouse;
  MouseMessage mouse_msg;

  mouse_msg.message = MessageMouse;
  mouse_msg.x = mouse_event->x;
  mouse_msg.y = mouse_event->y;

  mouse_msg.flags = 0;
  int32 type = event->type;
  int32 button = mouse_event->button;
  if (type == NPEventType_MouseDown) {
    if (button == NPMouseButton_Left) {
      mouse_msg.flags |= LeftDown;
    } else if (button == NPMouseButton_Right) {
      mouse_msg.flags |= RightDown;
    }
  } else if (type == NPEventType_MouseUp) {
    if (button == NPMouseButton_Left) {
      mouse_msg.flags |= LeftUp;
    } else if (button == NPMouseButton_Right) {
      mouse_msg.flags |= RightUp;
    }
  }
  host_->write_data((const char*)&mouse_msg, sizeof(mouse_msg));
}

// Process the pending update command from the host.
// Return true if the screen image has been updated.
bool ChromotingClient::handle_update_message() {
  UpdateMessage update_message;
  int result = host_->read_data(reinterpret_cast<char*>(&update_message),
                                sizeof(update_message));
  if (!result) {
    std::cout << "Failed to get update command" << std::endl;
    return false;
  }

  if (update_message.message != MessageUpdate) {
    std::cout << "Expected MessageUpdate" << std::endl;
    return false;
  }

  if (verbose_) {
    std::cout << "message: " << update_message.message << std::endl;
  }

  if (update_message.compression == CompressionZlib) {
    // Read all data.
    ZDecompressor decomp;
    char buffer[4096];
    int size = update_message.compressed_size;
    while (size > 0) {
      // Determine how much we should read from network.
      int read = std::min(static_cast<int>(sizeof(buffer)), size);
      result = host_->read_data(buffer, read);
      decomp.Write(buffer, read);
      size -= read;
    }
    decomp.Flush();

    // Decompress raw image data and break into individual images.
    char* raw_buffer = decomp.GetRawData();
    int raw_size = decomp.GetRawSize();
    int read = 0;
    BinaryImageHeader header;
    while (read < raw_size) {
      memcpy(&header, raw_buffer, sizeof(BinaryImageHeader));
      if (!check_image_header(&header)) {
        return false;
      }
      read += sizeof(BinaryImageHeader);
      raw_buffer += sizeof(BinaryImageHeader);

      // Merge this image fragment into the screen bitmap.
      merge_image(&header, raw_buffer);

      read += header.size;
      raw_buffer += header.size;
    }
  } else if (update_message.compression == CompressionNone) {
    printf("compressionNone\n");
    for (int i = 0; i < update_message.num_diffs; i++) {
      BinaryImage* image = new BinaryImage();
      read_image(image);

      // Merge this image update into the screen image.
      merge_image(&image->header, image->data.get());

      delete image;
    }
  } else {
    return false;
  }

  return true;
}

// Check the validity of the image header.
bool ChromotingClient::check_image_header(BinaryImageHeader* header) {
  if (header == NULL) {
    std::cout << "Invalid image" << std::endl;
    return false;
  }

  if (header->format != FormatRaw && header->format != FormatVp8) {
    std::cout << "Wrong image format : " << header->format << std::endl;
    return false;
  }

  if (verbose_) {
    std::cout << "Image:" << std::endl;
    std::cout << "  Format " << header->format << std::endl;
    std::cout << "  X,Y " << header->x << ", "
              << header->y << std::endl;
    std::cout << "  WxH " << header->width << " x "
              << header->height << std::endl;
    std::cout << "  Size " << header->size << std::endl;
  }

  return true;
}

// Read an image from the host and store it in the given BinaryImage.
bool ChromotingClient::read_image(BinaryImage* image) {
  int result = host_->read_data(reinterpret_cast<char*>(&image->header),
                                sizeof(image->header));
  if (!result) {
    std::cout << "Failed to receive image header" << std::endl;
    return false;
  }

  if (!check_image_header(&image->header)) {
    return false;
  }

  char* raw_data = new char[image->header.size];
  result = host_->read_data(raw_data, image->header.size);
  if (!result) {
    std::cout << "Failed to receive image data" << std::endl;
    return false;
  }

  if (image->header.format == FormatRaw) {
    // Raw image - all we need to do is load the data, so we're done.
    image->data.reset(raw_data);
    return true;
  } else if (image->header.format == FormatVp8) {
    return false;
    // TODO(hclam): Enable this block of code when we have VP8.
#if 0
    // Vp8 encoded - need to convert YUV image data to RGB.
    static VP8VideoDecoder decoder;
    uint8* planes[3];
    int strides[3];
    printf("decoder.DecodeFrame\n");
    if (!decoder.DecodeFrame(raw_data, image->header.size)) {
      std::cout << "Unable to decode frame" << std::endl;
      return false;
    }
    printf("decoder.GetDecodedFrame\n");
    if (!decoder.GetDecodedFrame(reinterpret_cast<char**>(planes), strides)) {
      std::cout << "Unable to get decoded frame" << std::endl;
      return false;
    }
    printf("width = %d\n", image->header.width);
    for (int i=0; i<3; i++) {
      printf("stride[%d] = %d\n", i, strides[0]);
    }

    // Convert YUV to RGB.
    int width = image->header.width;
    int height = image->header.height;
    char* rgb_data = new char[width * height * sizeof(int32)];
    printf("ConvertYUVToRGB32\n");
    ConvertYUVToRGB32(planes[0], planes[1], planes[2],
                      reinterpret_cast<uint8*>(rgb_data),
                      width,
                      image->header.height,
                      width, // y stride,
                      width / 2,  // uv stride,
                      width * sizeof(int32),  // rgb stride
                      media::YV12);
    printf("conversion done\n");

    image->data.reset(rgb_data);

    // Raw YUV data is no longer needed.
    delete raw_data;
    return true;
#endif
  }

  return false;
}

}  // namespace remoting
