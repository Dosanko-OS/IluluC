/*
 * semantic.h - Análise semântica do IluluC
 *
 * Primeira passagem sobre a AST: registra símbolos, valida declarações
 * e constrói a tabela de símbolos para o type checker.
 */

#ifndef ILULUC_SEMANTIC_H
#define ILULUC_SEMANTIC_H

#include "ast.h"
#include "symbol_table.h"
#include "error.h"

/* ===== Contexto semântico ===== */
typedef struct {
    TabelaSimbolos *tabela;
    ContextoErros  *erros;
    int             tem_main;
    int             tem_erros;
} ContextoSemantico;

/* ---------- Interface pública ---------- */

/* Executa a análise semântica completa sobre o programa */
ContextoSemantico *semantica_analisar(No *programa, ContextoErros *erros);

/* Libera o contexto semântico (não libera a AST) */
void semantica_destruir(ContextoSemantico *ctx);

#endif /* ILULUC_SEMANTIC_H */
