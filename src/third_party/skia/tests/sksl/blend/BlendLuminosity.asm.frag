OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %sk_FragColor %sk_Clockwise %src %dst
OpExecutionMode %main OriginUpperLeft
OpName %sk_FragColor "sk_FragColor"
OpName %sk_Clockwise "sk_Clockwise"
OpName %src "src"
OpName %dst "dst"
OpName %main "main"
OpName %_1_alpha "_1_alpha"
OpName %_2_sda "_2_sda"
OpName %_3_dsa "_3_dsa"
OpName %_4_blend_set_color_luminance "_4_blend_set_color_luminance"
OpName %_5_lum "_5_lum"
OpName %_6_result "_6_result"
OpName %_7_minComp "_7_minComp"
OpName %_8_maxComp "_8_maxComp"
OpName %_9_d "_9_d"
OpName %_10_n "_10_n"
OpName %_11_d "_11_d"
OpDecorate %sk_FragColor RelaxedPrecision
OpDecorate %sk_FragColor Location 0
OpDecorate %sk_FragColor Index 0
OpDecorate %sk_Clockwise RelaxedPrecision
OpDecorate %sk_Clockwise BuiltIn FrontFacing
OpDecorate %src RelaxedPrecision
OpDecorate %dst RelaxedPrecision
OpDecorate %18 RelaxedPrecision
OpDecorate %20 RelaxedPrecision
OpDecorate %22 RelaxedPrecision
OpDecorate %26 RelaxedPrecision
OpDecorate %28 RelaxedPrecision
OpDecorate %32 RelaxedPrecision
OpDecorate %34 RelaxedPrecision
OpDecorate %44 RelaxedPrecision
OpDecorate %46 RelaxedPrecision
OpDecorate %48 RelaxedPrecision
OpDecorate %49 RelaxedPrecision
OpDecorate %50 RelaxedPrecision
OpDecorate %56 RelaxedPrecision
OpDecorate %58 RelaxedPrecision
OpDecorate %60 RelaxedPrecision
OpDecorate %65 RelaxedPrecision
OpDecorate %67 RelaxedPrecision
OpDecorate %69 RelaxedPrecision
OpDecorate %72 RelaxedPrecision
OpDecorate %77 RelaxedPrecision
OpDecorate %78 RelaxedPrecision
OpDecorate %84 RelaxedPrecision
OpDecorate %85 RelaxedPrecision
OpDecorate %86 RelaxedPrecision
OpDecorate %87 RelaxedPrecision
OpDecorate %88 RelaxedPrecision
OpDecorate %89 RelaxedPrecision
OpDecorate %92 RelaxedPrecision
OpDecorate %93 RelaxedPrecision
OpDecorate %94 RelaxedPrecision
OpDecorate %98 RelaxedPrecision
OpDecorate %99 RelaxedPrecision
OpDecorate %103 RelaxedPrecision
OpDecorate %104 RelaxedPrecision
OpDecorate %111 RelaxedPrecision
OpDecorate %112 RelaxedPrecision
OpDecorate %115 RelaxedPrecision
OpDecorate %116 RelaxedPrecision
OpDecorate %117 RelaxedPrecision
OpDecorate %120 RelaxedPrecision
OpDecorate %121 RelaxedPrecision
OpDecorate %122 RelaxedPrecision
OpDecorate %123 RelaxedPrecision
OpDecorate %124 RelaxedPrecision
OpDecorate %125 RelaxedPrecision
OpDecorate %131 RelaxedPrecision
OpDecorate %132 RelaxedPrecision
OpDecorate %133 RelaxedPrecision
OpDecorate %135 RelaxedPrecision
OpDecorate %136 RelaxedPrecision
OpDecorate %137 RelaxedPrecision
OpDecorate %138 RelaxedPrecision
OpDecorate %140 RelaxedPrecision
OpDecorate %141 RelaxedPrecision
OpDecorate %142 RelaxedPrecision
OpDecorate %146 RelaxedPrecision
OpDecorate %148 RelaxedPrecision
OpDecorate %150 RelaxedPrecision
OpDecorate %151 RelaxedPrecision
OpDecorate %152 RelaxedPrecision
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
%sk_FragColor = OpVariable %_ptr_Output_v4float Output
%bool = OpTypeBool
%_ptr_Input_bool = OpTypePointer Input %bool
%sk_Clockwise = OpVariable %_ptr_Input_bool Input
%_ptr_Input_v4float = OpTypePointer Input %v4float
%src = OpVariable %_ptr_Input_v4float Input
%dst = OpVariable %_ptr_Input_v4float Input
%void = OpTypeVoid
%14 = OpTypeFunction %void
%_ptr_Function_float = OpTypePointer Function %float
%v3float = OpTypeVector %float 3
%_ptr_Function_v3float = OpTypePointer Function %v3float
%float_0_300000012 = OpConstant %float 0.300000012
%float_0_589999974 = OpConstant %float 0.589999974
%float_0_109999999 = OpConstant %float 0.109999999
%43 = OpConstantComposite %v3float %float_0_300000012 %float_0_589999974 %float_0_109999999
%false = OpConstantFalse %bool
%float_0 = OpConstant %float 0
%float_1 = OpConstant %float 1
%main = OpFunction %void None %14
%15 = OpLabel
%_1_alpha = OpVariable %_ptr_Function_float Function
%_2_sda = OpVariable %_ptr_Function_v3float Function
%_3_dsa = OpVariable %_ptr_Function_v3float Function
%_4_blend_set_color_luminance = OpVariable %_ptr_Function_v3float Function
%_5_lum = OpVariable %_ptr_Function_float Function
%_6_result = OpVariable %_ptr_Function_v3float Function
%_7_minComp = OpVariable %_ptr_Function_float Function
%_8_maxComp = OpVariable %_ptr_Function_float Function
%_9_d = OpVariable %_ptr_Function_float Function
%_10_n = OpVariable %_ptr_Function_v3float Function
%_11_d = OpVariable %_ptr_Function_float Function
%18 = OpLoad %v4float %dst
%19 = OpCompositeExtract %float %18 3
%20 = OpLoad %v4float %src
%21 = OpCompositeExtract %float %20 3
%22 = OpFMul %float %19 %21
OpStore %_1_alpha %22
%26 = OpLoad %v4float %src
%27 = OpVectorShuffle %v3float %26 %26 0 1 2
%28 = OpLoad %v4float %dst
%29 = OpCompositeExtract %float %28 3
%30 = OpVectorTimesScalar %v3float %27 %29
OpStore %_2_sda %30
%32 = OpLoad %v4float %dst
%33 = OpVectorShuffle %v3float %32 %32 0 1 2
%34 = OpLoad %v4float %src
%35 = OpCompositeExtract %float %34 3
%36 = OpVectorTimesScalar %v3float %33 %35
OpStore %_3_dsa %36
%44 = OpLoad %v3float %_2_sda
%39 = OpDot %float %43 %44
OpStore %_5_lum %39
%46 = OpLoad %float %_5_lum
%48 = OpLoad %v3float %_3_dsa
%47 = OpDot %float %43 %48
%49 = OpFSub %float %46 %47
%50 = OpLoad %v3float %_3_dsa
%51 = OpCompositeConstruct %v3float %49 %49 %49
%52 = OpFAdd %v3float %51 %50
OpStore %_6_result %52
%56 = OpLoad %v3float %_6_result
%57 = OpCompositeExtract %float %56 0
%58 = OpLoad %v3float %_6_result
%59 = OpCompositeExtract %float %58 1
%55 = OpExtInst %float %1 FMin %57 %59
%60 = OpLoad %v3float %_6_result
%61 = OpCompositeExtract %float %60 2
%54 = OpExtInst %float %1 FMin %55 %61
OpStore %_7_minComp %54
%65 = OpLoad %v3float %_6_result
%66 = OpCompositeExtract %float %65 0
%67 = OpLoad %v3float %_6_result
%68 = OpCompositeExtract %float %67 1
%64 = OpExtInst %float %1 FMax %66 %68
%69 = OpLoad %v3float %_6_result
%70 = OpCompositeExtract %float %69 2
%63 = OpExtInst %float %1 FMax %64 %70
OpStore %_8_maxComp %63
%72 = OpLoad %float %_7_minComp
%74 = OpFOrdLessThan %bool %72 %float_0
OpSelectionMerge %76 None
OpBranchConditional %74 %75 %76
%75 = OpLabel
%77 = OpLoad %float %_5_lum
%78 = OpLoad %float %_7_minComp
%79 = OpFOrdNotEqual %bool %77 %78
OpBranch %76
%76 = OpLabel
%80 = OpPhi %bool %false %15 %79 %75
OpSelectionMerge %82 None
OpBranchConditional %80 %81 %82
%81 = OpLabel
%84 = OpLoad %float %_5_lum
%85 = OpLoad %float %_7_minComp
%86 = OpFSub %float %84 %85
OpStore %_9_d %86
%87 = OpLoad %float %_5_lum
%88 = OpLoad %v3float %_6_result
%89 = OpLoad %float %_5_lum
%90 = OpCompositeConstruct %v3float %89 %89 %89
%91 = OpFSub %v3float %88 %90
%92 = OpLoad %float %_5_lum
%93 = OpLoad %float %_9_d
%94 = OpFDiv %float %92 %93
%95 = OpVectorTimesScalar %v3float %91 %94
%96 = OpCompositeConstruct %v3float %87 %87 %87
%97 = OpFAdd %v3float %96 %95
OpStore %_6_result %97
OpBranch %82
%82 = OpLabel
%98 = OpLoad %float %_8_maxComp
%99 = OpLoad %float %_1_alpha
%100 = OpFOrdGreaterThan %bool %98 %99
OpSelectionMerge %102 None
OpBranchConditional %100 %101 %102
%101 = OpLabel
%103 = OpLoad %float %_8_maxComp
%104 = OpLoad %float %_5_lum
%105 = OpFOrdNotEqual %bool %103 %104
OpBranch %102
%102 = OpLabel
%106 = OpPhi %bool %false %82 %105 %101
OpSelectionMerge %109 None
OpBranchConditional %106 %107 %108
%107 = OpLabel
%111 = OpLoad %v3float %_6_result
%112 = OpLoad %float %_5_lum
%113 = OpCompositeConstruct %v3float %112 %112 %112
%114 = OpFSub %v3float %111 %113
%115 = OpLoad %float %_1_alpha
%116 = OpLoad %float %_5_lum
%117 = OpFSub %float %115 %116
%118 = OpVectorTimesScalar %v3float %114 %117
OpStore %_10_n %118
%120 = OpLoad %float %_8_maxComp
%121 = OpLoad %float %_5_lum
%122 = OpFSub %float %120 %121
OpStore %_11_d %122
%123 = OpLoad %float %_5_lum
%124 = OpLoad %v3float %_10_n
%125 = OpLoad %float %_11_d
%127 = OpFDiv %float %float_1 %125
%128 = OpVectorTimesScalar %v3float %124 %127
%129 = OpCompositeConstruct %v3float %123 %123 %123
%130 = OpFAdd %v3float %129 %128
OpStore %_4_blend_set_color_luminance %130
OpBranch %109
%108 = OpLabel
%131 = OpLoad %v3float %_6_result
OpStore %_4_blend_set_color_luminance %131
OpBranch %109
%109 = OpLabel
%132 = OpLoad %v3float %_4_blend_set_color_luminance
%133 = OpLoad %v4float %dst
%134 = OpVectorShuffle %v3float %133 %133 0 1 2
%135 = OpFAdd %v3float %132 %134
%136 = OpLoad %v3float %_3_dsa
%137 = OpFSub %v3float %135 %136
%138 = OpLoad %v4float %src
%139 = OpVectorShuffle %v3float %138 %138 0 1 2
%140 = OpFAdd %v3float %137 %139
%141 = OpLoad %v3float %_2_sda
%142 = OpFSub %v3float %140 %141
%143 = OpCompositeExtract %float %142 0
%144 = OpCompositeExtract %float %142 1
%145 = OpCompositeExtract %float %142 2
%146 = OpLoad %v4float %src
%147 = OpCompositeExtract %float %146 3
%148 = OpLoad %v4float %dst
%149 = OpCompositeExtract %float %148 3
%150 = OpFAdd %float %147 %149
%151 = OpLoad %float %_1_alpha
%152 = OpFSub %float %150 %151
%153 = OpCompositeConstruct %v4float %143 %144 %145 %152
OpStore %sk_FragColor %153
OpReturn
OpFunctionEnd
