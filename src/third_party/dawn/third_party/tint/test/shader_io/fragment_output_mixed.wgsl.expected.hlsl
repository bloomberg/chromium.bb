struct FragmentOutputs {
  int loc0;
  float frag_depth;
  uint loc1;
  float loc2;
  uint sample_mask;
  float4 loc3;
};
struct tint_symbol {
  int loc0 : SV_Target0;
  uint loc1 : SV_Target1;
  float loc2 : SV_Target2;
  float4 loc3 : SV_Target3;
  float frag_depth : SV_Depth;
  uint sample_mask : SV_Coverage;
};

FragmentOutputs main_inner() {
  const FragmentOutputs tint_symbol_1 = {1, 2.0f, 1u, 1.0f, 2u, float4(1.0f, 2.0f, 3.0f, 4.0f)};
  return tint_symbol_1;
}

tint_symbol main() {
  const FragmentOutputs inner_result = main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.loc0 = inner_result.loc0;
  wrapper_result.frag_depth = inner_result.frag_depth;
  wrapper_result.loc1 = inner_result.loc1;
  wrapper_result.loc2 = inner_result.loc2;
  wrapper_result.sample_mask = inner_result.sample_mask;
  wrapper_result.loc3 = inner_result.loc3;
  return wrapper_result;
}
