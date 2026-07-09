#include "stdio.h"

int loop(int a, int b, int c) {
  int i, ret = 0;


  for (i = a; i < b; i++) {
      int x = 4+a;
    int k = x*c;
    int k2 = k + 5;
    printf("%d", k2);
  }

  return ret;
}