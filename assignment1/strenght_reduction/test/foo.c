#include <stdint.h>

extern void print();

typedef uint32_t i32;
//using prints to separate muls
void test(i32 x, i32 y, i32 z, int sd){
    i32 a = x*23;
    print();
    i32 b = y*24;
    print();
    i32 c = x*64;
    print();
    i32 d = z/16;
    print();
    int s = sd/16;
}