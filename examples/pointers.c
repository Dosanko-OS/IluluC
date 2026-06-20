/* Gerado por IluluC a partir de examples/pointers.ilc */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

int main(void);

int main(void) {
    int32_t valor = 42;
    int32_t *ptr = &(valor);
    int32_t lido = *(ptr);
    return 0;
}

