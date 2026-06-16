#include <stdint.h>

typedef uint32_t i32;
void foo (i32 x, i32 y, i32 z){
    i32 a = x + 0;
    i32 at = 0 + a;
    i32 att = at + 10;

    i32 b = y-0;
    i32 bt = b+10;

    i32 c = 1*z;
    i32 ct = c+10;
}