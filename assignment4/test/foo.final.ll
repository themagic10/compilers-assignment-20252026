; ModuleID = 'foo.simple.ll'
source_filename = "foo.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind sspstrong uwtable
define dso_local void @fun(ptr noundef %0, ptr noundef %1, i32 noundef %2) #0 {
  br label %4

4:                                                ; preds = %12, %3
  %.03 = phi i32 [ 15, %3 ], [ %9, %12 ]
  %.01 = phi i32 [ 0, %3 ], [ %13, %12 ]
  %5 = phi i32 [ 16, %3 ], [ %17, %12 ]
  %6 = icmp slt i32 %.01, 10
  %7 = icmp slt i32 %.01, 10
  br i1 %6, label %8, label %25

8:                                                ; preds = %4
  %9 = add nsw i32 %.03, 1
  %10 = sext i32 %.01 to i64
  %11 = getelementptr inbounds i32, ptr %0, i64 %10
  store i32 3, ptr %11, align 4
  br label %16

12:                                               ; preds = %16
  %13 = add nsw i32 %.01, 1
  br label %4, !llvm.loop !6

14:                                               ; No predecessors!
  br label %15

15:                                               ; preds = %23, %14
  %.02 = phi i32 [ 16, %14 ], [ %17, %23 ]
  br i1 %7, label %16, label %25

16:                                               ; preds = %8, %15
  %17 = add nsw i32 %5, 1
  %18 = sext i32 %.01 to i64
  %19 = getelementptr inbounds i32, ptr %0, i64 %18
  %20 = load i32, ptr %19, align 4
  %21 = sext i32 %.01 to i64
  %22 = getelementptr inbounds i32, ptr %1, i64 %21
  store i32 %20, ptr %22, align 4
  br label %12

23:                                               ; No predecessors!
  %24 = add nsw i32 %.01, 1
  br label %15, !llvm.loop !8

25:                                               ; preds = %4, %15
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
