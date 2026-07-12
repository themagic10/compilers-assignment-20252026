; ModuleID = 'foo.m2r.ll'
source_filename = "foo.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind sspstrong uwtable
define dso_local void @fun(ptr noundef %0, ptr noundef %1) #0 {
  br label %3

3:                                                ; preds = %9, %2
  %.02 = phi i32 [ undef, %2 ], [ %6, %9 ]
  %.01 = phi i32 [ 0, %2 ], [ %10, %9 ]
  %4 = icmp slt i32 %.01, 10
  br i1 %4, label %5, label %11

5:                                                ; preds = %3
  %6 = add nsw i32 %.02, 1
  %7 = sext i32 %.01 to i64
  %8 = getelementptr inbounds i32, ptr %0, i64 %7
  store i32 3, ptr %8, align 4
  br label %9

9:                                                ; preds = %5
  %10 = add nsw i32 %.01, 1
  br label %3, !llvm.loop !6

11:                                               ; preds = %3
  br label %12

12:                                               ; preds = %22, %11
  %.03 = phi i32 [ 0, %11 ], [ %15, %22 ]
  %.0 = phi i32 [ 0, %11 ], [ %23, %22 ]
  %13 = icmp slt i32 %.0, 10
  br i1 %13, label %14, label %24

14:                                               ; preds = %12
  %15 = add nsw i32 %.03, 1
  %16 = add nsw i32 %.0, 1
  %17 = sext i32 %16 to i64
  %18 = getelementptr inbounds i32, ptr %0, i64 %17
  %19 = load i32, ptr %18, align 4
  %20 = sext i32 %.0 to i64
  %21 = getelementptr inbounds i32, ptr %1, i64 %20
  store i32 %19, ptr %21, align 4
  br label %22

22:                                               ; preds = %14
  %23 = add nsw i32 %.0, 1
  br label %12, !llvm.loop !8

24:                                               ; preds = %12
  ret void
}

attributes #0 = { noinline nounwind sspstrong uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 19.1.7"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
!8 = distinct !{!8, !7}
