// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINTING_INFO_WIN_H_
#define PRINTING_BACKEND_PRINTING_INFO_WIN_H_

#include <objidl.h>
#include <winspool.h>

#include "base/memory/scoped_ptr.h"
#include "printing/printing_export.h"

namespace printing {

namespace internal {

PRINTING_EXPORT uint8* GetDriverInfo(HANDLE printer, int level);
PRINTING_EXPORT uint8* GetPrinterInfo(HANDLE printer, int level);

// This class is designed to work with PRINTER_INFO_X structures
// and calls GetPrinter internally with correctly allocated buffer.
template <typename PrinterInfoType, int level>
class PrinterInfo {
 public:
  bool Init(HANDLE printer) {
    buffer_.reset(GetPrinterInfo(printer, level));
    return buffer_;
  }

  const PrinterInfoType* get() const {
    return reinterpret_cast<const PrinterInfoType*>(buffer_.get());
  }

 private:
  scoped_array<uint8> buffer_;
};

// This class is designed to work with DRIVER_INFO_X structures
// and calls GetDriverInfo internally with correctly allocated buffer.
template <typename DriverInfoType, int level>
class DriverInfo {
 public:
  bool Init(HANDLE printer) {
    buffer_.reset(GetDriverInfo(printer, level));
    return buffer_;
  }

  const DriverInfoType* get() const {
    return reinterpret_cast<const DriverInfoType*>(buffer_.get());
  }

 private:
  scoped_array<uint8> buffer_;
};

}  // namespace internal

typedef internal::PrinterInfo<PRINTER_INFO_2, 2> PrinterInfo2;
typedef internal::PrinterInfo<PRINTER_INFO_5, 5> PrinterInfo5;
typedef internal::PrinterInfo<PRINTER_INFO_8, 8> PrinterInfo8;
typedef internal::PrinterInfo<PRINTER_INFO_9, 9> PrinterInfo9;

typedef internal::DriverInfo<DRIVER_INFO_6, 6> DriverInfo6;

// Retrieves DEVMODE from PRINTER_INFO_* structures.
// Requests in following order:
//    9 (user-default),
//    8 (admin-default),
//    2 (printer-default).
class PRINTING_EXPORT UserDefaultDevMode {
 public:
  UserDefaultDevMode();
  ~UserDefaultDevMode();

  bool Init(HANDLE printer);

  const DEVMODE* get() const {
    return dev_mode_;
  }

 private:
  PrinterInfo2 info_2_;
  PrinterInfo8 info_8_;
  PrinterInfo9 info_9_;
  const DEVMODE* dev_mode_;
};

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINTING_INFO_WIN_H_
