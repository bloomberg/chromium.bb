// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <assert.h>
#include <windows.h>
#include <wininet.h>

#include <fstream>

#include "client/windows/sender/crash_report_sender.h"

namespace google_airbag {

using std::ifstream;
using std::ios;

static const wchar_t kUserAgent[] = L"Airbag/1.0 (Windows)";

// Helper class which closes an internet handle when it goes away
class CrashReportSender::AutoInternetHandle {
 public:
  explicit AutoInternetHandle(HINTERNET handle) : handle_(handle) {}
  ~AutoInternetHandle() {
    if (handle_) {
      InternetCloseHandle(handle_);
    }
  }

  HINTERNET get() { return handle_; }

 private:
  HINTERNET handle_;
};

// static
bool CrashReportSender::SendCrashReport(
    const wstring &url, const map<wstring, wstring> &parameters,
    const wstring &dump_file_name) {
  // TODO(bryner): support non-ASCII parameter names
  if (!CheckParameters(parameters)) {
    return false;
  }

  // Break up the URL and make sure we can handle it
  wchar_t scheme[16], host[256], path[256];
  URL_COMPONENTS components;
  memset(&components, 0, sizeof(components));
  components.dwStructSize = sizeof(components);
  components.lpszScheme = scheme;
  components.dwSchemeLength = sizeof(scheme);
  components.lpszHostName = host;
  components.dwHostNameLength = sizeof(host);
  components.lpszUrlPath = path;
  components.dwUrlPathLength = sizeof(path);
  if (!InternetCrackUrl(url.c_str(), static_cast<DWORD>(url.size()),
                        0, &components)) {
    return false;
  }
  if (wcscmp(scheme, L"http") != 0) {
    return false;
  }

  AutoInternetHandle internet(InternetOpen(kUserAgent,
                                           INTERNET_OPEN_TYPE_DIRECT,
                                           NULL,  // proxy name
                                           NULL,  // proxy bypass
                                           0));   // flags
  if (!internet.get()) {
    return false;
  }

  AutoInternetHandle connection(InternetConnect(internet.get(),
                                                host,
                                                components.nPort,
                                                NULL,    // user name
                                                NULL,    // password
                                                INTERNET_SERVICE_HTTP,
                                                0,       // flags
                                                NULL));  // context
  if (!connection.get()) {
    return false;
  }

  AutoInternetHandle request(HttpOpenRequest(connection.get(),
                                             L"POST",
                                             path,
                                             NULL,    // version
                                             NULL,    // referer
                                             NULL,    // agent type
                                             0,       // flags
                                             NULL));  // context
  if (!request.get()) {
    return false;
  }

  wstring boundary = GenerateMultipartBoundary();
  wstring content_type_header = GenerateRequestHeader(boundary);
  HttpAddRequestHeaders(request.get(),
                        content_type_header.c_str(),
                        -1, HTTP_ADDREQ_FLAG_ADD);

  string request_body;
  GenerateRequestBody(parameters, dump_file_name, boundary, &request_body);

  // The explicit comparison to TRUE avoids a warning (C4800).
  return (HttpSendRequest(request.get(), NULL, 0,
                          const_cast<char *>(request_body.data()),
                          static_cast<DWORD>(request_body.size())) == TRUE);
}

// static
wstring CrashReportSender::GenerateMultipartBoundary() {
  // The boundary has 27 '-' characters followed by 16 hex digits
  static const wchar_t kBoundaryPrefix[] = L"---------------------------";
  static const int kBoundaryLength = 27 + 16 + 1;

  // Generate some random numbers to fill out the boundary
  int r0 = rand();
  int r1 = rand();

  wchar_t temp[kBoundaryLength];
  swprintf_s(temp, kBoundaryLength, L"%s%08X%08X", kBoundaryPrefix, r0, r1);
  return wstring(temp);
}

// static
wstring CrashReportSender::GenerateRequestHeader(const wstring &boundary) {
  wstring header = L"Content-Type: multipart/form-data; boundary=";
  header += boundary;
  return header;
}

// static
bool CrashReportSender::GenerateRequestBody(
    const map<wstring, wstring> &parameters,
    const wstring &minidump_filename, const wstring &boundary,
    string *request_body) {
  vector<char> contents;
  GetFileContents(minidump_filename, &contents);
  if (contents.empty()) {
    return false;
  }

  string boundary_str = WideToUTF8(boundary);
  if (boundary_str.empty()) {
    return false;
  }

  request_body->clear();

  // Append each of the parameter pairs as a form-data part
  for (map<wstring, wstring>::const_iterator pos = parameters.begin();
       pos != parameters.end(); ++pos) {
    request_body->append("--" + boundary_str + "\r\n");
    request_body->append("Content-Disposition: form-data; name=\"" +
                         WideToUTF8(pos->first) + "\"\r\n\r\n" +
                         WideToUTF8(pos->second) + "\r\n");
  }

  // Now append the minidump file as a binary (octet-stream) part
  string filename_utf8 = WideToUTF8(minidump_filename);
  if (filename_utf8.empty()) {
    return false;
  }

  request_body->append("--" + boundary_str + "\r\n");
  request_body->append("Content-Disposition: form-data; "
                       "name=\"upload_file_minidump\"; "
                       "filename=\"" + filename_utf8 + "\"\r\n");
  request_body->append("Content-Type: application/octet-stream\r\n");
  request_body->append("\r\n");

  request_body->append(&(contents[0]), contents.size());
  request_body->append("\r\n");
  request_body->append("--" + boundary_str + "--\r\n");
  return true;
}

// static
void CrashReportSender::GetFileContents(const wstring &filename,
                                        vector<char> *contents) {
  ifstream file;
  file.open(filename.c_str(), ios::binary);
  if (file.is_open()) {
    file.seekg(0, ios::end);
    int length = file.tellg();
    contents->resize(length);
    file.seekg(0, ios::beg);
    file.read(&((*contents)[0]), length);
    file.close();
  } else {
    contents->clear();
  }
}

// static
string CrashReportSender::WideToUTF8(const wstring &wide) {
  if (wide.length() == 0) {
    return string();
  }

  // compute the length of the buffer we'll need
  int charcount = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1,
                                      NULL, 0, NULL, NULL);
  if (charcount == 0) {
    return string();
  }

  // convert
  char *buf = new char[charcount];
  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, buf, charcount,
                      NULL, NULL);

  string result(buf);
  delete[] buf;
  return result;
}

// static
bool CrashReportSender::CheckParameters(
    const map<wstring, wstring> &parameters) {
  for (map<wstring, wstring>::const_iterator pos = parameters.begin();
       pos != parameters.end(); ++pos) {
    const wstring &str = pos->first;
    if (str.size() == 0) {
      return false;  // disallow empty parameter names
    }
    for (unsigned int i = 0; i < str.size(); ++i) {
      wchar_t c = str[i];
      if (c < 32 || c == '"' || c > 127) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace google_airbag
