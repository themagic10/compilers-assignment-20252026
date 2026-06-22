#include <stdint.h>

typedef uint32_t i32;

void foo(i32 x, i32 y, i32 z){
    i32 a = x+2;
    i32 b = a + 5;
    i32 c = b + a;
}