/* Gerado por IluluC a partir de examples/functions.ilc */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

int32_t soma(int32_t a, int32_t b);
int main(void);

int32_t soma(int32_t a, int32_t b) {
    return (a + b);
}

int main(void) {
    int32_t resultado = soma(10, 32);
    puts("Resultado: 42");
    return 0;
}

