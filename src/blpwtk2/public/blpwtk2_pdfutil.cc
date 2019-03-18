/*
 * Copyright (C) 2016 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_pdfutil.h>
#include <build/build_config.h>
#include <pdf/pdf.h>

namespace blpwtk2 {

bool PdfUtil::RenderPDFPageToDC(const void* pdf_buffer,
                                int buffer_size,
                                int page_number,
                                NativeDeviceContext dc,
                                int dpi,
                                int bounds_origin_x,
                                int bounds_origin_y,
                                int bounds_width,
                                int bounds_height,
                                bool fit_to_bounds,
                                bool stretch_to_bounds,
                                bool keep_aspect_ratio,
                                bool center_in_bounds,
                                bool autorotate,
                                bool use_color)
{
    return chrome_pdf::RenderPDFPageToDC(base::span<const uint8_t>((const uint8_t*)pdf_buffer, buffer_size),
                                         page_number,
                                         dc,
                                         dpi,
                                         dpi,
                                         bounds_origin_x,
                                         bounds_origin_y,
                                         bounds_width,
                                         bounds_height,
                                         fit_to_bounds,
                                         stretch_to_bounds,
                                         keep_aspect_ratio,
                                         center_in_bounds,
                                         autorotate,
                                         use_color);
}

}  // close namespace blpwtk2

// vim: ts=4 et

