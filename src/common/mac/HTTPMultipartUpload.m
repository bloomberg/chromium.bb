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

#import "HTTPMultipartUpload.h"

@interface HTTPMultipartUpload(PrivateMethods)
- (NSString *)multipartBoundary;
- (NSData *)formDataForKey:(NSString *)key value:(NSString *)value;
- (NSData *)formDataForFile:(NSString *)file name:(NSString *)name;
@end

@implementation HTTPMultipartUpload
//=============================================================================
#pragma mark -
#pragma mark || Private ||
//=============================================================================
- (NSString *)multipartBoundary {
  // The boundary has 27 '-' characters followed by 16 hex digits
  return [NSString stringWithFormat:@"---------------------------%08X%08X",
    rand(), rand()];
}

//=============================================================================
- (NSData *)formDataForKey:(NSString *)key value:(NSString *)value {
  NSString *escaped = 
    [key stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
  NSString *fmt =
    @"--%@\r\nContent-Disposition: form-data; name=\"%@\"\r\n\r\n%@\r\n";
  NSString *form = [NSString stringWithFormat:fmt, boundary_, escaped, value];

  return [form dataUsingEncoding:NSUTF8StringEncoding];
}

//=============================================================================
- (NSData *)formDataForFile:(NSString *)file name:(NSString *)name {
  NSMutableData *data = [NSMutableData data];
  NSString *escaped = 
    [name stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
  NSString *fmt = @"--%@\r\nContent-Disposition: form-data; name=\"%@\"; "
    "filename=\"%@\"\r\nContent-Type: application/octet-stream\r\n\r\n";
  NSString *pre = [NSString stringWithFormat:fmt, boundary_, escaped,
    [file lastPathComponent]];
  NSString *post = [NSString stringWithFormat:@"\r\n--%@--\r\n", boundary_];

  [data appendData:[pre dataUsingEncoding:NSUTF8StringEncoding]];
  [data appendData:[NSData dataWithContentsOfFile:file]];
  [data appendData:[post dataUsingEncoding:NSUTF8StringEncoding]];

  return data;
}

//=============================================================================
#pragma mark -
#pragma mark || Public ||
//=============================================================================
- (id)initWithURL:(NSURL *)url {
  if ((self = [super init])) {
    url_ = [url copy];
    boundary_ = [[self multipartBoundary] retain];
  }

  return self;
}

//=============================================================================
- (void)dealloc {
  [url_ release];
  [parameters_ release];
  [files_ release];
  [boundary_ release];
  [response_ release];

  [super dealloc];
}

//=============================================================================
- (NSURL *)URL {
  return url_;
}

//=============================================================================
- (void)setParameters:(NSDictionary *)parameters {
  if (parameters != parameters_) {
    [parameters_ release];
    parameters_ = [parameters copy];
  }
}

//=============================================================================
- (NSDictionary *)parameters {
  return parameters_;
}

//=============================================================================
- (void)addFileAtPath:(NSString *)path name:(NSString *)name {
  if (!files_)
    files_ = [[NSMutableDictionary alloc] init];

  [files_ setObject:path forKey:name];
}

//=============================================================================
- (NSDictionary *)files {
  return files_;
}

//=============================================================================
- (NSData *)send:(NSError **)error {
  NSMutableURLRequest *req = [[NSMutableURLRequest alloc] initWithURL:url_];
  NSMutableData *postBody = [NSMutableData data];
  int i, count;

  [req setValue:[NSString stringWithFormat:@"multipart/form-data; boundary=%@",
    boundary_] forHTTPHeaderField:@"Content-type"];

  // Add any parameters to the message
  NSArray *parameterKeys = [parameters_ allKeys];
  NSString *key;

  count = [parameterKeys count];
  for (i = 0; i < count; ++i) {
    key = [parameterKeys objectAtIndex:i];
    [postBody appendData:[self formDataForKey:key
                                        value:[parameters_ objectForKey:key]]];
  }

  // Add any files to the message
  NSArray *fileNames = [files_ allKeys];
  count = [fileNames count];
  for (i = 0; i < count; ++i) {
    NSString *name = [fileNames objectAtIndex:i];
    NSString *file = [files_ objectForKey:name];

    [postBody appendData:[self formDataForFile:file name:name]];
  }

  [req setHTTPBody:postBody];
  [req setHTTPMethod:@"POST"];

  return [NSURLConnection sendSynchronousRequest:req
                               returningResponse:&response_
                                           error:error];
}

//=============================================================================
- (NSHTTPURLResponse *)response {
  return response_;
}

@end
