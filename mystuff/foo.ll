; Declare the string constant as a global constant.
@str = private unnamed_addr constant [13 x i8] c"hello world\0A\00"
@x = global constant i32 42
@y = global constant i64 *@x

; External declaration of the puts function
declare i32 @puts(ptr nocapture) nounwind

; Definition of main function
define i32 @main() {
  ; Call puts function to write out the string to stdout.
  call i32 @puts(ptr @str)
  i64 
  ret i32 0
}

; Named metadata
; !0 = !{i32 42, null, !"string"}
; !foo = !{!0}
; define i1024 @add(i1024 %a, i1024 %b) {
;   %1 = add i1024 %a, %b
;   ret i1024 %1
; }