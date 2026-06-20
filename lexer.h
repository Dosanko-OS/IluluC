/*
 * lexer.h - Interface do analisador léxico do IluluC
 *
 * O lexer converte o texto fonte em uma sequência de tokens.
 * Cada chamada a lexer_proximo() devolve o próximo token.
 */

#ifndef ILULUC_LEXER_H
#define ILULUC_LEXER_H

#include "token.h"
#include "error.h"

/* ===== Estado do analisador léxico ===== */
typedef struct {
    const char    *fonte;    /* Texto fonte completo (não modificado)  */
    int            pos;      /* Posição atual no texto (índice)        */
    int            linha;    /* Linha atual (começa em 1)              */
    int            coluna;   /* Coluna atual (começa em 1)             */
    const char    *arquivo;  /* Nome do arquivo (para erros)           */
    ContextoErros *erros;    /* Contexto para relatar erros léxicos    */
} Lexer;

/* ---------- Funções públicas ---------- */

/* Inicializa o lexer com o texto fonte */
void lexer_init(Lexer *lex,
                const char *fonte,
                const char *arquivo,
                ContextoErros *erros);

/* Retorna o próximo token (avança no texto) */
Token lexer_proximo(Lexer *lex);

/* Espera no próximo token sem avançar (peek) */
Token lexer_espiar(Lexer *lex);

#endif /* ILULUC_LEXER_H */