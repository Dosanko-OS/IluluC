/*
 * parser.h - Interface do analisador sintático do IluluC
 *
 * O parser consome tokens do lexer e produz a AST completa.
 * Usa descida recursiva com 1 token de lookahead.
 */

#ifndef ILULUC_PARSER_H
#define ILULUC_PARSER_H

#include "lexer.h"
#include "ast.h"
#include "error.h"

/* ===== Estado do parser ===== */
typedef struct {
    Lexer         *lex;          /* Fonte de tokens                  */
    Token          atual;        /* Token atual (sendo processado)   */
    Token          proximo;      /* Lookahead de 1 token             */
    ContextoErros *erros;        /* Contexto para relatar erros      */
    int            profundidade; /* Profundidade de blocos (loops)   */
    int            em_funcao;    /* Estamos dentro de uma função?    */
} Parser;

/* ---------- Funções públicas ---------- */

/* Inicializa o parser com o lexer e contexto de erros */
void parser_init(Parser *p, Lexer *lex, ContextoErros *erros);

/* Faz o parse completo e retorna o nó raiz (NO_PROGRAMA) */
No *parser_parsear(Parser *p);

/* Libera recursos do parser (os tokens armazenados) */
void parser_destruir(Parser *p);

#endif /* ILULUC_PARSER_H */