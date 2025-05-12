; RUN: opt < %s -passes=rel-lookup-table-converter -relocation-model=pic -mtriple=x86_64-apple-darwin -S | FileCheck -check-prefix=NO-UNNAMED-ADDR %s
; RUN: opt < %s -passes=rel-lookup-table-converter -relocation-model=pic -mtriple=x86_64 -S | FileCheck -check-prefix=UNNAMED-ADDR %s
; RUN: opt < %s -passes=rel-lookup-table-converter -relocation-model=pic -mtriple=aarch64-apple-darwin -S | FileCheck -check-prefix=UNNAMED-ADDR %s

; NO-UNNAMED-ADDR: @L{{.*}} = private constant
; UNNAMED-ADDR: @L{{.*}} = private unnamed_addr constant

@L1 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@L2 = private unnamed_addr constant [8 x i8] c"\01\02\03\04\05\06\07\08", align 1
@L3 = private unnamed_addr constant [1 x i8] c"\09", align 1
@switch.table.broken = private unnamed_addr constant [3 x ptr] [ptr @L1, ptr @L2, ptr @L3], align 8
@switch.table.broken.1 = private unnamed_addr constant [3 x ptr] [ptr getelementptr inbounds ([1 x i8], ptr @L1, i64 1, i64 0), ptr getelementptr inbounds ([8 x i8], ptr @L2, i64 1, i64 0), ptr getelementptr inbounds ([1 x i8], ptr @L3, i64 1, i64 0)], align 8

define i64 @broken(i64 %0) {
  %2 = getelementptr inbounds [3 x ptr], ptr @switch.table.broken, i64 0, i64 %0
  %3 = load ptr, ptr %2, align 8
  %4 = getelementptr inbounds [3 x ptr], ptr @switch.table.broken.1, i64 0, i64 %0
  %5 = load ptr, ptr %4, align 8
  %6 = tail call i64 @slice_len_from_ptr_end(ptr %3, ptr %5)
  ret i64 %6
}

declare i64 @slice_len_from_ptr_end(ptr, ptr)
