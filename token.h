/*
 * token.h - Tipos e estrutura de tokens da linguagem IluluC
 *
 * Define todos os tokens que o lexer pode produzir e o parser pode consumir.
 */

#ifndef ILULUC_TOKEN_H
#define ILULUC_TOKEN_H

#include "error.h"

/* ===== Todos os tipos de token ===== */
typedef enum {

    /* --- Fim de arquivo --- */
    TOKEN_EOF = 0,

    /* --- Literais --- */
    TOKEN_LIT_INT,      /* 42, 0, 255                      */
    TOKEN_LIT_FLOAT,    /* 3.14, 0.5                       */
    TOKEN_LIT_STRING,   /* "olá mundo"                     */
    TOKEN_LIT_CHAR,     /* 'a', '\n'                       */

    /* --- Identificador genérico --- */
    TOKEN_IDENT,        /* nome, variavel, MinhaStruct      */

    /* --- Palavras-chave de controle de fluxo --- */
    TOKEN_IF,           /* if                               */
    TOKEN_ELSE,         /* else                             */
    TOKEN_WHILE,        /* while                            */
    TOKEN_LOOP,         /* loop                             */
    TOKEN_FOREVER,      /* forever                          */
    TOKEN_FROM,         /* from                             */
    TOKEN_TO,           /* to                               */
    TOKEN_BREAK,        /* break                            */
    TOKEN_CONTINUE,     /* continue                         */
    TOKEN_RETURN,       /* return                           */
    TOKEN_MATCH,        /* match                            */

    /* --- Palavras-chave de declaração --- */
    TOKEN_FUNCTION,     /* function                         */
    TOKEN_STRUCT,       /* struct                           */
    TOKEN_ENUM,         /* enum                             */
    TOKEN_CONST,        /* const                            */
    TOKEN_USE,          /* use                              */
    TOKEN_NAMESPACE,    /* namespace                        */
    TOKEN_TYPE,         /* type                             */
    TOKEN_EXTERN,       /* extern                           */
    TOKEN_UNSAFE,       /* unsafe                           */

    /* --- Palavras-chave de ponteiro/memória --- */
    TOKEN_POINTER,      /* pointer                          */
    TOKEN_ADDRESS,      /* address                          */
    TOKEN_DEREF,        /* deref                            */
    TOKEN_AS,           /* as                               */

    /* --- Literais booleanos e nulo --- */
    TOKEN_TRUE,         /* true                             */
    TOKEN_FALSE,        /* false                            */
    TOKEN_NULL,         /* null                             */

    /* --- Tipos primitivos --- */
    TOKEN_U8,           /* u8                               */
    TOKEN_U16,          /* u16                              */
    TOKEN_U32,          /* u32                              */
    TOKEN_U64,          /* u64                              */
    TOKEN_I8,           /* i8                               */
    TOKEN_I16,          /* i16                              */
    TOKEN_I32,          /* i32                              */
    TOKEN_I64,          /* i64                              */
    TOKEN_F32,          /* f32                              */
    TOKEN_F64,          /* f64                              */
    TOKEN_BOOL,         /* bool                             */
    TOKEN_CHAR,         /* char  (tipo)                     */
    TOKEN_STRING,       /* string (tipo)                    */
    TOKEN_VOID,         /* void                             */

    /* --- Operadores aritméticos --- */
    TOKEN_MAIS,         /* +                                */
    TOKEN_MENOS,        /* -                                */
    TOKEN_STAR,         /* *                                */
    TOKEN_BARRA,        /* /                                */
    TOKEN_PORCENTO,     /* %                                */

    /* --- Operadores relacionais --- */
    TOKEN_IGUAL_IGUAL,  /* ==                               */
    TOKEN_DIFERENTE,    /* !=                               */
    TOKEN_MAIOR,        /* >                                */
    TOKEN_MENOR,        /* <                                */
    TOKEN_MAIOR_IGUAL,  /* >=                               */
    TOKEN_MENOR_IGUAL,  /* <=                               */

    /* --- Operadores lógicos --- */
    TOKEN_E_E,          /* &&                               */
    TOKEN_OU_OU,        /* ||                               */
    TOKEN_EXCLAMACAO,   /* !                                */

    /* --- Atribuição --- */
    TOKEN_IGUAL,        /* =                                */

    /* --- Delimitadores --- */
    TOKEN_ABRE_CHAVE,   /* {                                */
    TOKEN_FECHA_CHAVE,  /* }                                */
    TOKEN_ABRE_PAREN,   /* (                                */
    TOKEN_FECHA_PAREN,  /* )                                */
    TOKEN_ABRE_COL,     /* [                                */
    TOKEN_FECHA_COL,    /* ]                                */
    TOKEN_VIRGULA,      /* ,                                */
    TOKEN_DOIS_PONTOS,  /* :                                */
    TOKEN_SETA,         /* ->                               */
    TOKEN_FAT_SETA,     /* =>                               */
    TOKEN_PONTO,        /* .                                */

    /* Sentinela: total de tipos de token */
    TOKEN__TOTAL

} TipoToken;

/* ===== Estrutura de um token ===== */
typedef struct {
    TipoToken   tipo;         /* Categoria do token                    */
    char       *lexema;       /* Texto original (alocado dinamicamente) */
    Localizacao loc;          /* Onde está no código fonte             */

    /* Valores para literais (evita re-parse) */
    union {
        long long   valor_int;   /* Preenchido para TOKEN_LIT_INT   */
        double      valor_float; /* Preenchido para TOKEN_LIT_FLOAT */
    };
} Token;

/* ---------- Funções públicas ---------- */

/* Retorna o nome legível de um tipo de token */
const char *token_nome(TipoToken tipo);

/* Cria um token simples (sem valor literal) */
Token token_criar(TipoToken tipo, const char *lexema, Localizacao loc);

/* Libera memória alocada no token */
void token_destruir(Token *t);

/* Retorna 1 se o token é um tipo primitivo */
int token_eh_tipo(TipoToken tipo);

/* Retorna 1 se o token é um operador binário */
int token_eh_op_binario(TipoToken tipo);

#endif /* ILULUC_TOKEN_H */