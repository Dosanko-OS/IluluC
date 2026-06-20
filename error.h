/*
 * error.h - Sistema de tratamento de erros do compilador IluluC
 *
 * Fornece tipos e funções para reportar erros léxicos, sintáticos,
 * semânticos e de tipo com localização completa (arquivo, linha, coluna).
 */

#ifndef ILULUC_ERROR_H
#define ILULUC_ERROR_H

#include <stdarg.h>

/* Tamanho máximo de uma mensagem de erro */
#define ERRO_MSG_MAX       512

/* Capacidade inicial do array de erros */
#define ERROS_CAP_INICIAL  16

/* ===== Categorias de erro ===== */
typedef enum {
    ERRO_LEXICO,      /* Caractere inválido, literal mal formado, etc. */
    ERRO_SINTATICO,   /* Token inesperado, estrutura mal formada, etc. */
    ERRO_SEMANTICO,   /* Variável não declarada, redeclaração, etc.    */
    ERRO_TIPO,        /* Incompatibilidade de tipos, etc.               */
    ERRO_INTERNO      /* Erro interno do compilador (bug)               */
} TipoErro;

/* ===== Posição no código fonte ===== */
typedef struct {
    const char *arquivo; /* Nome do arquivo fonte                */
    int         linha;   /* Número da linha   (começa em 1)      */
    int         coluna;  /* Número da coluna  (começa em 1)      */
} Localizacao;

/* ===== Um erro individual ===== */
typedef struct {
    TipoErro    tipo;
    Localizacao loc;
    char        mensagem[ERRO_MSG_MAX];
} Erro;

/* ===== Coletor de erros (passado por todo o compilador) ===== */
typedef struct {
    Erro *erros;       /* Array dinâmico de erros  */
    int   quantidade;  /* Quantidade de erros       */
    int   capacidade;  /* Capacidade alocada        */
    int   tem_fatal;   /* Há erro interno/fatal?    */
} ContextoErros;

/* ---------- Funções públicas ---------- */

/* Cria um contexto de erros vazio */
ContextoErros *erros_criar(void);

/* Libera o contexto de erros */
void erros_destruir(ContextoErros *ctx);

/* Adiciona um erro formatado (printf-style) ao contexto */
void erros_adicionar(ContextoErros *ctx,
                     TipoErro tipo,
                     Localizacao loc,
                     const char *fmt, ...);

/* Imprime todos os erros em stderr */
void erros_imprimir(const ContextoErros *ctx);

/* Retorna 1 se houver pelo menos um erro registrado */
int erros_tem_erros(const ContextoErros *ctx);

/* Constrói uma Localizacao */
Localizacao loc_criar(const char *arquivo, int linha, int coluna);

/* Retorna o nome legível de um TipoErro */
const char *erro_tipo_nome(TipoErro tipo);

#endif /* ILULUC_ERROR_H */