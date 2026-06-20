/*
 * ast.h - Árvore Sintática Abstrata (AST) do IluluC
 *
 * Define todos os tipos de nós e a estrutura No (tagged union).
 * Cada construção da linguagem corresponde a um tipo de nó.
 */

#ifndef ILULUC_AST_H
#define ILULUC_AST_H

#include "token.h"
#include "error.h"

/* Declaração antecipada para referência circular */
typedef struct No        No;
typedef struct TipoIluluC TipoIluluC;

/* ===== Categorias de tipo (usadas em NO_TIPO e no type checker) ===== */
typedef enum {
    TIPO_U8, TIPO_U16, TIPO_U32, TIPO_U64,
    TIPO_I8, TIPO_I16, TIPO_I32, TIPO_I64,
    TIPO_F32, TIPO_F64,
    TIPO_BOOL,
    TIPO_CHAR,
    TIPO_STRING,
    TIPO_VOID,
    TIPO_PONTEIRO,    /* pointer<T>    */
    TIPO_ARRAY,       /* T[N]          */
    TIPO_GENERICO,    /* list<T>       */
    TIPO_STRUCT,      /* struct nome   */
    TIPO_ENUM,        /* enum nome     */
    TIPO_ALIAS,       /* type nome = T */
    TIPO_DESCONHECIDO /* para recuperação de erros */
} CategoriaType;

/* ===== Tipos de nó da AST ===== */
typedef enum {

    /* --- Nó raiz --- */
    NO_PROGRAMA,

    /* --- Declarações de nível superior e de bloco --- */
    NO_FUNCAO_DECL,    /* function foo(...) -> T { } */
    NO_PARAM,          /* nome: tipo                 */
    NO_VAR_DECL,       /* tipo nome = expr           */
    NO_CONST_DECL,     /* const NOME = expr          */
    NO_STRUCT_DECL,    /* struct Nome { ... }        */
    NO_STRUCT_CAMPO,   /* nome: tipo                 */
    NO_ENUM_DECL,      /* enum Nome { A, B, C }      */
    NO_ALIAS_TIPO,     /* type byte = u8             */
    NO_NAMESPACE,      /* namespace ns { ... }       */
    NO_EXTERN_DECL,    /* extern func(...)           */
    NO_USE_DECL,       /* use modulo                 */

    /* --- Statements --- */
    NO_BLOCO,          /* { stmt* }                           */
    NO_IF,             /* if cond { } else { }                */
    NO_WHILE,          /* while cond { }                      */
    NO_LOOP_FOREVER,   /* loop forever { }                    */
    NO_LOOP_FOR,       /* loop i from 0 to 100 { }            */
    NO_BREAK,          /* break                               */
    NO_CONTINUE,       /* continue                            */
    NO_RETURN,         /* return expr?                        */
    NO_MATCH,          /* match expr { padrao => corpo }      */
    NO_MATCH_BRACO,    /* padrao => corpo                     */
    NO_UNSAFE,         /* unsafe { }                          */
    NO_EXPR_STMT,      /* expr como statement                 */

    /* --- Expressões --- */
    NO_BINARIO,        /* esq op dir          */
    NO_UNARIO,         /* op operando         */
    NO_CHAMADA,        /* func(arg1, arg2)    */
    NO_INDEX,          /* array[indice]       */
    NO_CAMPO_ACESSO,   /* objeto.campo        */
    NO_CAST,           /* as<tipo>(expr)      */
    NO_ADDRESS,        /* address(expr)       */
    NO_DEREF,          /* deref(expr)         */
    NO_IDENT,          /* nome                */
    NO_LIT_INT,        /* 42                  */
    NO_LIT_FLOAT,      /* 3.14                */
    NO_LIT_STRING,     /* "texto"             */
    NO_LIT_CHAR,       /* 'a'                 */
    NO_LIT_BOOL,       /* true / false        */
    NO_LIT_NULL,       /* null                */

    /* --- Nó de tipo (aparece em declarações) --- */
    NO_TIPO,

} TipoNo;

/* ===== Estrutura de tipo (preenchida pelo type checker) ===== */
struct TipoIluluC {
    CategoriaType categoria;
    struct TipoIluluC *subtipo;  /* ponteiro, array ou parâmetro genérico */
    int                tamanho;  /* para TIPO_ARRAY                        */
    char              *nome;     /* para tipos nomeados                    */
};

/* ===== Nó da AST ===== */
struct No {
    TipoNo      tipo;          /* Discriminante da union                   */
    Localizacao loc;           /* Localização no código fonte              */
    TipoIluluC *tipo_inferido; /* Preenchido pelo type checker (NULL antes) */

    union {

        /* ----- NO_PROGRAMA ----- */
        struct {
            No **decls;
            int  n_decls;
        } programa;

        /* ----- NO_FUNCAO_DECL ----- */
        struct {
            char *nome;
            No  **params;
            int   n_params;
            No   *tipo_ret;  /* NULL = void */
            No   *corpo;
        } funcao;

        /* ----- NO_PARAM ----- */
        struct {
            char *nome;
            No   *tipo;
        } param;

        /* ----- NO_VAR_DECL ----- */
        struct {
            char *nome;
            No   *tipo;   /* pode ser NULL se tipo for inferido */
            No   *valor;  /* inicializador (pode ser NULL)      */
        } var;

        /* ----- NO_CONST_DECL ----- */
        struct {
            char *nome;
            No   *valor;
        } constante;

        /* ----- NO_STRUCT_DECL ----- */
        struct {
            char *nome;
            No  **campos;
            int   n_campos;
        } estrutura;

        /* ----- NO_STRUCT_CAMPO ----- */
        struct {
            char *nome;
            No   *tipo;
        } campo;

        /* ----- NO_ENUM_DECL ----- */
        struct {
            char  *nome;
            char **variantes;
            int    n_variantes;
        } enumeracao;

        /* ----- NO_ALIAS_TIPO ----- */
        struct {
            char *nome;
            No   *tipo;
        } alias;

        /* ----- NO_NAMESPACE ----- */
        struct {
            char *nome;
            No  **itens;
            int   n_itens;
        } espaco;

        /* ----- NO_EXTERN_DECL ----- */
        struct {
            char *nome;
            No  **params;
            int   n_params;
            No   *tipo_ret; /* NULL = void */
        } externo;

        /* ----- NO_USE_DECL ----- */
        struct {
            char *modulo;
        } uso;

        /* ----- NO_BLOCO ----- */
        struct {
            No **stmts;
            int  n_stmts;
        } bloco;

        /* ----- NO_IF ----- */
        struct {
            No *cond;
            No *entao;
            No *senao;  /* NULL se sem else */
        } se;

        /* ----- NO_WHILE ----- */
        struct {
            No *cond;
            No *corpo;
        } enquanto;

        /* ----- NO_LOOP_FOREVER ----- */
        struct {
            No *corpo;
        } loop_eterno;

        /* ----- NO_LOOP_FOR ----- */
        struct {
            char *var;
            No   *inicio;
            No   *fim;
            No   *corpo;
        } loop_for;

        /* ----- NO_RETURN ----- */
        struct {
            No *valor; /* NULL se return; sem valor */
        } retorno;

        /* ----- NO_MATCH ----- */
        struct {
            No  *expr;
            No **bracos;
            int  n_bracos;
        } match;

        /* ----- NO_MATCH_BRACO ----- */
        struct {
            char *padrao;  /* nome do enum variant */
            No   *corpo;   /* expressão ou bloco   */
        } braco;

        /* ----- NO_UNSAFE ----- */
        struct {
            No *bloco;
        } bloco_unsafe;

        /* ----- NO_EXPR_STMT ----- */
        struct {
            No *expr;
        } expr_stmt;

        /* ----- NO_BINARIO ----- */
        struct {
            TipoToken op;
            No       *esq;
            No       *dir;
        } binario;

        /* ----- NO_UNARIO ----- */
        struct {
            TipoToken op;
            No       *operando;
        } unario;

        /* ----- NO_CHAMADA ----- */
        struct {
            No  *funcao; /* NO_IDENT ou NO_CAMPO_ACESSO */
            No **args;
            int  n_args;
        } chamada;

        /* ----- NO_INDEX ----- */
        struct {
            No *array;
            No *indice;
        } index;

        /* ----- NO_CAMPO_ACESSO ----- */
        struct {
            No   *objeto;
            char *campo;
        } campo_acesso;

        /* ----- NO_CAST ----- */
        struct {
            No *tipo;
            No *expr;
        } cast;

        /* ----- NO_ADDRESS / NO_DEREF ----- */
        struct {
            No *expr;
        } ref;

        /* ----- NO_IDENT ----- */
        struct {
            char *nome;
        } ident;

        /* ----- NO_LIT_INT ----- */
        struct {
            long long valor;
        } lit_int;

        /* ----- NO_LIT_FLOAT ----- */
        struct {
            double valor;
        } lit_float;

        /* ----- NO_LIT_STRING ----- */
        struct {
            char *valor;
        } lit_str;

        /* ----- NO_LIT_CHAR ----- */
        struct {
            char valor;
        } lit_char;

        /* ----- NO_LIT_BOOL ----- */
        struct {
            int valor; /* 0 = false, 1 = true */
        } lit_bool;

        /* ----- NO_TIPO ----- */
        struct {
            CategoriaType categoria;
            char         *nome;    /* para tipos nomeados e genéricos */
            No           *subtipo; /* para ponteiro, array, genérico  */
            int           tamanho; /* para array: u8[512] -> 512      */
        } tipo_no;

    }; /* union anônima — C11/C17 */
};

/* ---------- Funções públicas ---------- */

/* Aloca um nó com tipo e localização */
No *no_criar(TipoNo tipo, Localizacao loc);

/* Libera recursivamente toda a subárvore */
void no_destruir(No *no);

/* Imprime a AST em formato indentado (para debug) */
void no_imprimir(const No *no, int recuo);

/* Funções de criação para tipos comuns de nó */
No *no_lit_int   (long long valor,    Localizacao loc);
No *no_lit_float (double valor,       Localizacao loc);
No *no_lit_string(const char *valor,  Localizacao loc);
No *no_lit_char  (char valor,         Localizacao loc);
No *no_lit_bool  (int valor,          Localizacao loc);
No *no_lit_null  (Localizacao loc);
No *no_ident     (const char *nome,   Localizacao loc);

/* Tipo IluluC (para o type checker) */
TipoIluluC *tipo_criar(CategoriaType cat);
TipoIluluC *tipo_ponteiro(TipoIluluC *subtipo);
TipoIluluC *tipo_array(TipoIluluC *subtipo, int tamanho);
void        tipo_destruir(TipoIluluC *t);
int         tipo_igual(const TipoIluluC *a, const TipoIluluC *b);
int         tipo_eh_numerico(const TipoIluluC *t);
int         tipo_eh_inteiro(const TipoIluluC *t);
const char *tipo_nome_c(const TipoIluluC *t);
const char *tipo_nome_iluluc(const TipoIluluC *t);

#endif /* ILULUC_AST_H */
