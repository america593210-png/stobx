; ModuleID = 'StobxModule'
source_filename = "StobxModule"

@intFormat = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@floatFormat = private unnamed_addr constant [4 x i8] c"%f\0A\00", align 1
@charFormat = private unnamed_addr constant [4 x i8] c"%c\0A\00", align 1
@stringFormat = private unnamed_addr constant [4 x i8] c"%s\0A\00", align 1
@trueString = private unnamed_addr constant [5 x i8] c"true\00", align 1
@falseString = private unnamed_addr constant [6 x i8] c"false\00", align 1
@0 = private unnamed_addr constant [9 x i8] c"test.txt\00", align 1
@1 = private unnamed_addr constant [2 x i8] c"r\00", align 1

declare ptr @fopen(ptr, ptr)

declare i64 @fwrite(ptr, i64, i64, ptr)

declare i64 @fread(ptr, i64, i64, ptr)

declare i32 @fclose(ptr)

declare ptr @malloc(i64)

declare void @free(ptr)

define i32 @main() {
entry:
  %calltmp = call ptr @fopen(ptr @0, ptr @1)
  %rf = alloca ptr, align 8
  store ptr %calltmp, ptr %rf, align 8
  %calltmp1 = call ptr @malloc(i64 32)
  %buf = alloca ptr, align 8
  store ptr %calltmp1, ptr %buf, align 8
  %buf2 = load ptr, ptr %buf, align 8
  %rf3 = load ptr, ptr %rf, align 8
  %calltmp4 = call i64 @fread(ptr %buf2, i64 1, i64 11, ptr %rf3)
  %read_count = alloca i64, align 8
  store i64 %calltmp4, ptr %read_count, align 4
  %rf5 = load ptr, ptr %rf, align 8
  %calltmp6 = call i32 @fclose(ptr %rf5)
  %rclosed = alloca i32, align 4
  store i32 %calltmp6, ptr %rclosed, align 4
  ret i32 0
}

declare i32 @printf(ptr, ...)
