// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#include "untrusted/srpcgen/ppb_rpc.h"
#ifdef __native_client__
#include "native_client/src/shared/srpc/nacl_srpc.h"
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) do { (void) P; } while (0)
#endif  // UNREFERENCED_PARAMETER
#else
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#endif  // __native_client__

NaClSrpcError ObjectStubRpcClient::HasProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    int32_t* success,
    nacl_abi_size_t* exception_bytes, char* exception
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "HasProperty:CCC:iC",
      capability_bytes, capability,
      name_bytes, name,
      exception_in_bytes, exception_in,
      success,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::HasMethod(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    int32_t* success,
    nacl_abi_size_t* exception_bytes, char* exception
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "HasMethod:CCC:iC",
      capability_bytes, capability,
      name_bytes, name,
      exception_in_bytes, exception_in,
      success,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::GetProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* value_bytes, char* value,
    nacl_abi_size_t* exception_bytes, char* exception
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "GetProperty:CCC:CC",
      capability_bytes, capability,
      name_bytes, name,
      exception_in_bytes, exception_in,
      value_bytes, value,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::GetAllPropertyNames(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    int32_t* property_count,
    nacl_abi_size_t* properties_bytes, char* properties,
    nacl_abi_size_t* exception_bytes, char* exception
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "GetAllPropertyNames:CC:iCC",
      capability_bytes, capability,
      exception_in_bytes, exception_in,
      property_count,
      properties_bytes, properties,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::SetProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t value_bytes, char* value,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* exception_bytes, char* exception
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "SetProperty:CCCC:C",
      capability_bytes, capability,
      name_bytes, name,
      value_bytes, value,
      exception_in_bytes, exception_in,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::RemoveProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* exception_bytes, char* exception
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "RemoveProperty:CCC:C",
      capability_bytes, capability,
      name_bytes, name,
      exception_in_bytes, exception_in,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::Call(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    int32_t argc,
    nacl_abi_size_t argv_bytes, char* argv,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* ret_bytes, char* ret,
    nacl_abi_size_t* exception_bytes, char* exception
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "Call:CCiCC:CC",
      capability_bytes, capability,
      name_bytes, name,
      argc,
      argv_bytes, argv,
      exception_in_bytes, exception_in,
      ret_bytes, ret,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::Construct(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    int32_t argc,
    nacl_abi_size_t argv_bytes, char* argv,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* ret_bytes, char* ret,
    nacl_abi_size_t* exception_bytes, char* exception
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "Construct:CiCC:CC",
      capability_bytes, capability,
      argc,
      argv_bytes, argv,
      exception_in_bytes, exception_in,
      ret_bytes, ret,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::Deallocate(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "Deallocate:C:",
      capability_bytes, capability
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_AddRefResource(
    NaClSrpcChannel* channel,
    int64_t resource
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_AddRefResource:l:",
      resource
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_ReleaseResource(
    NaClSrpcChannel* channel,
    int64_t resource
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_ReleaseResource:l:",
      resource
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_GetTime(
    NaClSrpcChannel* channel,
    double* time
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_GetTime::d",
      time
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_Create(
    NaClSrpcChannel* channel,
    int64_t module,
    nacl_abi_size_t size_bytes, int32_t* size,
    int32_t is_always_opaque,
    int64_t* resource
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Create:lIi:l",
      module,
      size_bytes, size,
      is_always_opaque,
      resource
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_IsGraphics2D(
    NaClSrpcChannel* channel,
    int64_t resource,
    int32_t* success
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_IsGraphics2D:l:i",
      resource,
      success
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_Describe(
    NaClSrpcChannel* channel,
    int64_t graphics_2d,
    nacl_abi_size_t* size_bytes, int32_t* size,
    int32_t* is_always_opaque,
    int32_t* success
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Describe:l:Iii",
      graphics_2d,
      size_bytes, size,
      is_always_opaque,
      success
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_PaintImageData(
    NaClSrpcChannel* channel,
    int64_t graphics_2d,
    int64_t image,
    nacl_abi_size_t top_left_bytes, int32_t* top_left,
    nacl_abi_size_t src_rect_bytes, int32_t* src_rect
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_PaintImageData:llII:",
      graphics_2d,
      image,
      top_left_bytes, top_left,
      src_rect_bytes, src_rect
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_Scroll(
    NaClSrpcChannel* channel,
    int64_t graphics_2d,
    nacl_abi_size_t clip_rect_bytes, int32_t* clip_rect,
    nacl_abi_size_t amount_bytes, int32_t* amount
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Scroll:lII:",
      graphics_2d,
      clip_rect_bytes, clip_rect,
      amount_bytes, amount
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_ReplaceContents(
    NaClSrpcChannel* channel,
    int64_t graphics_2d,
    int64_t image
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_ReplaceContents:ll:",
      graphics_2d,
      image
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_GetNativeImageDataFormat(
    NaClSrpcChannel* channel,
    int32_t* format
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_GetNativeImageDataFormat::i",
      format
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_IsImageDataFormatSupported(
    NaClSrpcChannel* channel,
    int32_t format,
    int32_t* success
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_IsImageDataFormatSupported:i:i",
      format,
      success
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_Create(
    NaClSrpcChannel* channel,
    int64_t module,
    int32_t format,
    nacl_abi_size_t size_bytes, int32_t* size,
    int32_t init_to_zero,
    int64_t* resource
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_Create:liIi:l",
      module,
      format,
      size_bytes, size,
      init_to_zero,
      resource
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_IsImageData(
    NaClSrpcChannel* channel,
    int64_t resource,
    int32_t* success
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_IsImageData:l:i",
      resource,
      success
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_Describe(
    NaClSrpcChannel* channel,
    int64_t resource,
    nacl_abi_size_t* desc_bytes, int32_t* desc,
    int32_t* success
)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_Describe:l:Ii",
      resource,
      desc_bytes, desc,
      success
  );
  return retval;
}


