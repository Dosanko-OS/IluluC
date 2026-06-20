/* Gerado por IluluC a partir de examples/loops.ilc */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

int main(void);

int main(void) {
    int32_t i = 0;
    while ((i < 5))     {
        (i = (i + 1));
    }
    for (;;)     {
        break;
    }
    for (int32_t j = 0; j <= 10; ++j)     {
        continue;
    }
    return 0;
}

