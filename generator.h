/*
 * generator.h - Gerador de código C do IluluC
 *
 * Traduz a AST tipada para código C válido (C17).
 */

#ifndef ILULUC_GENERATOR_H
#define ILULUC_GENERATOR_H

#include <stdio.h>

#include "ast.h"
#include "semantic.h"

/* ===== Opções de geração ===== */
typedef struct {
    const char *arquivo_fonte; /* Nome do .ilc de origem (para comentário) */
} OpcoesGerador;

/* Gera código C a partir da AST tipada; retorna 0 em sucesso */
int gerador_emitir(No *programa,
                   ContextoSemantico *ctx,
                   FILE *saida,
                   const OpcoesGerador *opcoes);

#endif /* ILULUC_GENERATOR_H */
