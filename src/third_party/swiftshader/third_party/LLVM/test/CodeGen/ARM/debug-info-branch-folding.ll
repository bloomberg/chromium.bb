; RUN: llc < %s - | FileCheck %s
target datalayout = "e-p:32:32:32-i1:8:32-i8:8:32-i16:16:32-i32:32:32-i64:32:32-f32:32:32-f64:32:32-v64:32:64-v128:32:128-a0:0:32-n32"
target triple = "thumbv7-apple-macosx10.6.7"

;CHECK: 	vadd.f32	q4, q8, q8
;CHECK-NEXT: Ltmp
;CHECK-NEXT: 	@DEBUG_VALUE: y <- Q4+0
;CHECK-NEXT:    @DEBUG_VALUE: x <- Q4+0


@.str = external constant [13 x i8]

declare <4 x float> @test0001(float) nounwind readnone ssp

define i32 @main(i32 %argc, i8** nocapture %argv) nounwind ssp {
entry:
  br label %for.body9

for.body9:                                        ; preds = %for.body9, %entry
  %add19 = fadd <4 x float> undef, <float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float 1.000000e+00>, !dbg !39
  tail call void @llvm.dbg.value(metadata !{<4 x float> %add19}, i64 0, metadata !27), !dbg !39
  %add20 = fadd <4 x float> undef, <float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float 1.000000e+00>, !dbg !39
  tail call void @llvm.dbg.value(metadata !{<4 x float> %add20}, i64 0, metadata !28), !dbg !39
  br i1 undef, label %for.end54, label %for.body9, !dbg !44

for.end54:                                        ; preds = %for.body9
  %tmp115 = extractelement <4 x float> %add19, i32 1
  %conv6.i75 = fpext float %tmp115 to double, !dbg !45
  %call.i82 = tail call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([13 x i8]* @.str, i32 0, i32 0), double undef, double %conv6.i75, double undef, double undef) nounwind, !dbg !45
  %tmp116 = extractelement <4 x float> %add20, i32 1
  %conv6.i76 = fpext float %tmp116 to double, !dbg !45
  %call.i83 = tail call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([13 x i8]* @.str, i32 0, i32 0), double undef, double %conv6.i76, double undef, double undef) nounwind, !dbg !45
  ret i32 0, !dbg !49
}

declare i32 @printf(i8* nocapture, ...) nounwind

declare void @llvm.dbg.value(metadata, i64, metadata) nounwind readnone

!llvm.dbg.sp = !{!0, !10, !14}
!llvm.dbg.lv.test0001 = !{!18}
!llvm.dbg.lv.main = !{!19, !20, !24, !26, !27, !28, !29}
!llvm.dbg.lv.printFV = !{!30}

!0 = metadata !{i32 589870, i32 0, metadata !1, metadata !"test0001", metadata !"test0001", metadata !"", metadata !1, i32 3, metadata !3, i1 false, i1 true, i32 0, i32 0, i32 0, i32 256, i1 true, <4 x float> (float)* @test0001, null} ; [ DW_TAG_subprogram ]
!1 = metadata !{i32 589865, metadata !"build2.c", metadata !"/private/tmp", metadata !2} ; [ DW_TAG_file_type ]
!2 = metadata !{i32 589841, i32 0, i32 12, metadata !"build2.c", metadata !"/private/tmp", metadata !"clang version 3.0 (trunk 129915)", i1 true, i1 true, metadata !"", i32 0} ; [ DW_TAG_compile_unit ]
!3 = metadata !{i32 589845, metadata !1, metadata !"", metadata !1, i32 0, i64 0, i64 0, i32 0, i32 0, i32 0, metadata !4, i32 0, i32 0} ; [ DW_TAG_subroutine_type ]
!4 = metadata !{metadata !5}
!5 = metadata !{i32 589846, metadata !2, metadata !"v4f32", metadata !1, i32 14, i64 0, i64 0, i64 0, i32 0, metadata !6} ; [ DW_TAG_typedef ]
!6 = metadata !{i32 590083, metadata !2, metadata !"", metadata !2, i32 0, i64 128, i64 128, i32 0, i32 0, metadata !7, metadata !8, i32 0, i32 0} ; [ DW_TAG_vector_type ]
!7 = metadata !{i32 589860, metadata !2, metadata !"float", null, i32 0, i64 32, i64 32, i64 0, i32 0, i32 4} ; [ DW_TAG_base_type ]
!8 = metadata !{metadata !9}
!9 = metadata !{i32 589857, i64 0, i64 3}         ; [ DW_TAG_subrange_type ]
!10 = metadata !{i32 589870, i32 0, metadata !1, metadata !"main", metadata !"main", metadata !"", metadata !1, i32 59, metadata !11, i1 false, i1 true, i32 0, i32 0, i32 0, i32 256, i1 true, i32 (i32, i8**)* @main, null} ; [ DW_TAG_subprogram ]
!11 = metadata !{i32 589845, metadata !1, metadata !"", metadata !1, i32 0, i64 0, i64 0, i32 0, i32 0, i32 0, metadata !12, i32 0, i32 0} ; [ DW_TAG_subroutine_type ]
!12 = metadata !{metadata !13}
!13 = metadata !{i32 589860, metadata !2, metadata !"int", null, i32 0, i64 32, i64 32, i64 0, i32 0, i32 5} ; [ DW_TAG_base_type ]
!14 = metadata !{i32 589870, i32 0, metadata !15, metadata !"printFV", metadata !"printFV", metadata !"", metadata !15, i32 41, metadata !16, i1 true, i1 true, i32 0, i32 0, i32 0, i32 256, i1 true, null, null} ; [ DW_TAG_subprogram ]
!15 = metadata !{i32 589865, metadata !"/Volumes/Lalgate/work/llvm/projects/llvm-test/SingleSource/UnitTests/Vector/helpers.h", metadata !"/private/tmp", metadata !2} ; [ DW_TAG_file_type ]
!16 = metadata !{i32 589845, metadata !15, metadata !"", metadata !15, i32 0, i64 0, i64 0, i32 0, i32 0, i32 0, metadata !17, i32 0, i32 0} ; [ DW_TAG_subroutine_type ]
!17 = metadata !{null}
!18 = metadata !{i32 590081, metadata !0, metadata !"a", metadata !1, i32 16777219, metadata !7, i32 0} ; [ DW_TAG_arg_variable ]
!19 = metadata !{i32 590081, metadata !10, metadata !"argc", metadata !1, i32 16777275, metadata !13, i32 0} ; [ DW_TAG_arg_variable ]
!20 = metadata !{i32 590081, metadata !10, metadata !"argv", metadata !1, i32 33554491, metadata !21, i32 0} ; [ DW_TAG_arg_variable ]
!21 = metadata !{i32 589839, metadata !2, metadata !"", null, i32 0, i64 32, i64 32, i64 0, i32 0, metadata !22} ; [ DW_TAG_pointer_type ]
!22 = metadata !{i32 589839, metadata !2, metadata !"", null, i32 0, i64 32, i64 32, i64 0, i32 0, metadata !23} ; [ DW_TAG_pointer_type ]
!23 = metadata !{i32 589860, metadata !2, metadata !"char", null, i32 0, i64 8, i64 8, i64 0, i32 0, i32 6} ; [ DW_TAG_base_type ]
!24 = metadata !{i32 590080, metadata !25, metadata !"i", metadata !1, i32 60, metadata !13, i32 0} ; [ DW_TAG_auto_variable ]
!25 = metadata !{i32 589835, metadata !10, i32 59, i32 33, metadata !1, i32 14} ; [ DW_TAG_lexical_block ]
!26 = metadata !{i32 590080, metadata !25, metadata !"j", metadata !1, i32 60, metadata !13, i32 0} ; [ DW_TAG_auto_variable ]
!27 = metadata !{i32 590080, metadata !25, metadata !"x", metadata !1, i32 61, metadata !5, i32 0} ; [ DW_TAG_auto_variable ]
!28 = metadata !{i32 590080, metadata !25, metadata !"y", metadata !1, i32 62, metadata !5, i32 0} ; [ DW_TAG_auto_variable ]
!29 = metadata !{i32 590080, metadata !25, metadata !"z", metadata !1, i32 63, metadata !5, i32 0} ; [ DW_TAG_auto_variable ]
!30 = metadata !{i32 590081, metadata !14, metadata !"F", metadata !15, i32 16777257, metadata !31, i32 0} ; [ DW_TAG_arg_variable ]
!31 = metadata !{i32 589839, metadata !2, metadata !"", null, i32 0, i64 32, i64 32, i64 0, i32 0, metadata !32} ; [ DW_TAG_pointer_type ]
!32 = metadata !{i32 589846, metadata !2, metadata !"FV", metadata !15, i32 25, i64 0, i64 0, i64 0, i32 0, metadata !33} ; [ DW_TAG_typedef ]
!33 = metadata !{i32 589847, metadata !2, metadata !"", metadata !15, i32 22, i64 128, i64 128, i64 0, i32 0, i32 0, metadata !34, i32 0, i32 0} ; [ DW_TAG_union_type ]
!34 = metadata !{metadata !35, metadata !37}
!35 = metadata !{i32 589837, metadata !15, metadata !"V", metadata !15, i32 23, i64 128, i64 128, i64 0, i32 0, metadata !36} ; [ DW_TAG_member ]
!36 = metadata !{i32 589846, metadata !2, metadata !"v4sf", metadata !15, i32 3, i64 0, i64 0, i64 0, i32 0, metadata !6} ; [ DW_TAG_typedef ]
!37 = metadata !{i32 589837, metadata !15, metadata !"A", metadata !15, i32 24, i64 128, i64 32, i64 0, i32 0, metadata !38} ; [ DW_TAG_member ]
!38 = metadata !{i32 589825, metadata !2, metadata !"", metadata !2, i32 0, i64 128, i64 32, i32 0, i32 0, metadata !7, metadata !8, i32 0, i32 0} ; [ DW_TAG_array_type ]
!39 = metadata !{i32 79, i32 7, metadata !40, null}
!40 = metadata !{i32 589835, metadata !41, i32 75, i32 35, metadata !1, i32 18} ; [ DW_TAG_lexical_block ]
!41 = metadata !{i32 589835, metadata !42, i32 75, i32 5, metadata !1, i32 17} ; [ DW_TAG_lexical_block ]
!42 = metadata !{i32 589835, metadata !43, i32 71, i32 32, metadata !1, i32 16} ; [ DW_TAG_lexical_block ]
!43 = metadata !{i32 589835, metadata !25, i32 71, i32 3, metadata !1, i32 15} ; [ DW_TAG_lexical_block ]
!44 = metadata !{i32 75, i32 5, metadata !42, null}
!45 = metadata !{i32 42, i32 2, metadata !46, metadata !48}
!46 = metadata !{i32 589835, metadata !47, i32 42, i32 2, metadata !15, i32 20} ; [ DW_TAG_lexical_block ]
!47 = metadata !{i32 589835, metadata !14, i32 41, i32 28, metadata !15, i32 19} ; [ DW_TAG_lexical_block ]
!48 = metadata !{i32 95, i32 3, metadata !25, null}
!49 = metadata !{i32 99, i32 3, metadata !25, null}
