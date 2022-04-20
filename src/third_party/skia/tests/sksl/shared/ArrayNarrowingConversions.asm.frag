OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %_entrypoint_v "_entrypoint" %sk_FragColor %sk_Clockwise
OpExecutionMode %_entrypoint_v OriginUpperLeft
OpName %sk_FragColor "sk_FragColor"
OpName %sk_Clockwise "sk_Clockwise"
OpName %_UniformBuffer "_UniformBuffer"
OpMemberName %_UniformBuffer 0 "colorGreen"
OpMemberName %_UniformBuffer 1 "colorRed"
OpName %_entrypoint_v "_entrypoint_v"
OpName %main "main"
OpName %i2 "i2"
OpName %s2 "s2"
OpName %f2 "f2"
OpName %h2 "h2"
OpName %cf2 "cf2"
OpDecorate %sk_FragColor RelaxedPrecision
OpDecorate %sk_FragColor Location 0
OpDecorate %sk_FragColor Index 0
OpDecorate %sk_Clockwise BuiltIn FrontFacing
OpMemberDecorate %_UniformBuffer 0 Offset 0
OpMemberDecorate %_UniformBuffer 0 RelaxedPrecision
OpMemberDecorate %_UniformBuffer 1 Offset 16
OpMemberDecorate %_UniformBuffer 1 RelaxedPrecision
OpDecorate %_UniformBuffer Block
OpDecorate %10 Binding 0
OpDecorate %10 DescriptorSet 0
OpDecorate %_arr_int_int_2 ArrayStride 16
OpDecorate %s2 RelaxedPrecision
OpDecorate %34 RelaxedPrecision
OpDecorate %_arr_float_int_2 ArrayStride 16
OpDecorate %h2 RelaxedPrecision
OpDecorate %42 RelaxedPrecision
OpDecorate %43 RelaxedPrecision
OpDecorate %45 RelaxedPrecision
OpDecorate %50 RelaxedPrecision
OpDecorate %61 RelaxedPrecision
OpDecorate %83 RelaxedPrecision
OpDecorate %101 RelaxedPrecision
OpDecorate %103 RelaxedPrecision
OpDecorate %104 RelaxedPrecision
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
%sk_FragColor = OpVariable %_ptr_Output_v4float Output
%bool = OpTypeBool
%_ptr_Input_bool = OpTypePointer Input %bool
%sk_Clockwise = OpVariable %_ptr_Input_bool Input
%_UniformBuffer = OpTypeStruct %v4float %v4float
%_ptr_Uniform__UniformBuffer = OpTypePointer Uniform %_UniformBuffer
%10 = OpVariable %_ptr_Uniform__UniformBuffer Uniform
%void = OpTypeVoid
%15 = OpTypeFunction %void
%float_0 = OpConstant %float 0
%v2float = OpTypeVector %float 2
%19 = OpConstantComposite %v2float %float_0 %float_0
%_ptr_Function_v2float = OpTypePointer Function %v2float
%23 = OpTypeFunction %v4float %_ptr_Function_v2float
%int = OpTypeInt 32 1
%int_2 = OpConstant %int 2
%_arr_int_int_2 = OpTypeArray %int %int_2
%_ptr_Function__arr_int_int_2 = OpTypePointer Function %_arr_int_int_2
%int_1 = OpConstant %int 1
%_arr_float_int_2 = OpTypeArray %float %int_2
%_ptr_Function__arr_float_int_2 = OpTypePointer Function %_arr_float_int_2
%float_1 = OpConstant %float 1
%float_2 = OpConstant %float 2
%false = OpConstantFalse %bool
%_ptr_Function_v4float = OpTypePointer Function %v4float
%_ptr_Uniform_v4float = OpTypePointer Uniform %v4float
%int_0 = OpConstant %int 0
%_entrypoint_v = OpFunction %void None %15
%16 = OpLabel
%20 = OpVariable %_ptr_Function_v2float Function
OpStore %20 %19
%22 = OpFunctionCall %v4float %main %20
OpStore %sk_FragColor %22
OpReturn
OpFunctionEnd
%main = OpFunction %v4float None %23
%24 = OpFunctionParameter %_ptr_Function_v2float
%25 = OpLabel
%i2 = OpVariable %_ptr_Function__arr_int_int_2 Function
%s2 = OpVariable %_ptr_Function__arr_int_int_2 Function
%f2 = OpVariable %_ptr_Function__arr_float_int_2 Function
%h2 = OpVariable %_ptr_Function__arr_float_int_2 Function
%cf2 = OpVariable %_ptr_Function__arr_float_int_2 Function
%93 = OpVariable %_ptr_Function_v4float Function
%32 = OpCompositeConstruct %_arr_int_int_2 %int_1 %int_2
OpStore %i2 %32
%34 = OpCompositeConstruct %_arr_int_int_2 %int_1 %int_2
OpStore %s2 %34
%40 = OpCompositeConstruct %_arr_float_int_2 %float_1 %float_2
OpStore %f2 %40
%42 = OpCompositeConstruct %_arr_float_int_2 %float_1 %float_2
OpStore %h2 %42
%43 = OpLoad %_arr_int_int_2 %s2
OpStore %i2 %43
%44 = OpLoad %_arr_int_int_2 %i2
OpStore %s2 %44
%45 = OpLoad %_arr_float_int_2 %h2
OpStore %f2 %45
%46 = OpLoad %_arr_float_int_2 %f2
OpStore %h2 %46
OpStore %cf2 %40
%49 = OpLoad %_arr_int_int_2 %i2
%50 = OpLoad %_arr_int_int_2 %s2
%51 = OpCompositeExtract %int %49 0
%52 = OpCompositeExtract %int %50 0
%53 = OpIEqual %bool %51 %52
%54 = OpCompositeExtract %int %49 1
%55 = OpCompositeExtract %int %50 1
%56 = OpIEqual %bool %54 %55
%57 = OpLogicalAnd %bool %56 %53
OpSelectionMerge %59 None
OpBranchConditional %57 %58 %59
%58 = OpLabel
%60 = OpLoad %_arr_float_int_2 %f2
%61 = OpLoad %_arr_float_int_2 %h2
%62 = OpCompositeExtract %float %60 0
%63 = OpCompositeExtract %float %61 0
%64 = OpFOrdEqual %bool %62 %63
%65 = OpCompositeExtract %float %60 1
%66 = OpCompositeExtract %float %61 1
%67 = OpFOrdEqual %bool %65 %66
%68 = OpLogicalAnd %bool %67 %64
OpBranch %59
%59 = OpLabel
%69 = OpPhi %bool %false %25 %68 %58
OpSelectionMerge %71 None
OpBranchConditional %69 %70 %71
%70 = OpLabel
%72 = OpLoad %_arr_int_int_2 %i2
%73 = OpCompositeExtract %int %72 0
%74 = OpCompositeExtract %int %32 0
%75 = OpIEqual %bool %73 %74
%76 = OpCompositeExtract %int %72 1
%77 = OpCompositeExtract %int %32 1
%78 = OpIEqual %bool %76 %77
%79 = OpLogicalAnd %bool %78 %75
OpBranch %71
%71 = OpLabel
%80 = OpPhi %bool %false %59 %79 %70
OpSelectionMerge %82 None
OpBranchConditional %80 %81 %82
%81 = OpLabel
%83 = OpLoad %_arr_float_int_2 %h2
%84 = OpLoad %_arr_float_int_2 %cf2
%85 = OpCompositeExtract %float %83 0
%86 = OpCompositeExtract %float %84 0
%87 = OpFOrdEqual %bool %85 %86
%88 = OpCompositeExtract %float %83 1
%89 = OpCompositeExtract %float %84 1
%90 = OpFOrdEqual %bool %88 %89
%91 = OpLogicalAnd %bool %90 %87
OpBranch %82
%82 = OpLabel
%92 = OpPhi %bool %false %71 %91 %81
OpSelectionMerge %97 None
OpBranchConditional %92 %95 %96
%95 = OpLabel
%98 = OpAccessChain %_ptr_Uniform_v4float %10 %int_0
%101 = OpLoad %v4float %98
OpStore %93 %101
OpBranch %97
%96 = OpLabel
%102 = OpAccessChain %_ptr_Uniform_v4float %10 %int_1
%103 = OpLoad %v4float %102
OpStore %93 %103
OpBranch %97
%97 = OpLabel
%104 = OpLoad %v4float %93
OpReturnValue %104
OpFunctionEnd
