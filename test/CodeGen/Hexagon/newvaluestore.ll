; RUN: llc -march=hexagon -mcpu=hexagonv4 < %s | FileCheck %s
; Check that we generate new value store packet in V4

@i = global i32 0, align 4
@j = global i32 10, align 4
@k = global i32 100, align 4

define i32 @main() nounwind {
entry:
; CHECK: memw(r{{[0-9]+}}+#{{[0-9]+}}) = r{{[0-9]+}}.new
  %number1 = alloca i32, align 4
  %0 = load i32 , i32 * @i, align 4
  store i32 %0, i32* %number1, align 4
  ret i32 %0
}

