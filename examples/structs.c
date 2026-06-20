/* Gerado por IluluC a partir de examples/structs.ilc */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

int main(void);

typedef struct Ponto {
    int32_t x;
    int32_t y;
} Ponto;

int main(void) {
    struct Ponto p;
    (p.x = 10);
    (p.y = 20);
    return 0;
}

