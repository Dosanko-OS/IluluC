/*
 * symbol_table.h - Tabela de símbolos do IluluC
 *
 * Gerencia escopos aninhados (global, namespace, função, bloco)
 * e armazena variáveis, funções, structs, enums e aliases.
 */

#ifndef ILULUC_SYMBOL_TABLE_H
#define ILULUC_SYMBOL_TABLE_H

#include "ast.h"
#include "types.h"
#include "error.h"

/* ===== Categoria de símbolo ===== */
typedef enum {
    SIMB_VAR,
    SIMB_PARAM,
    SIMB_FUNCAO,
    SIMB_STRUCT,
    SIMB_ENUM,
    SIMB_ALIAS,
    SIMB_CONST,
    SIMB_EXTERN,
    SIMB_NAMESPACE
} CategoriaSimbolo;

/* ===== Um símbolo na tabela ===== */
typedef struct Simbolo {
    char            *nome;       /* Nome qualificado (ex: ns::foo)  */
    char            *nome_curto; /* Nome sem prefixo de namespace    */
    CategoriaSimbolo categoria;
    TipoIluluC      *tipo;       /* Tipo do símbolo                  */
    No              *no_decl;    /* Nó AST da declaração             */
    AssinaturaFuncao assinatura; /* Preenchido para funções/extern   */
    int              definido;   /* 1 se corpo/definição presente    */
} Simbolo;

/* ===== Um escopo léxico ===== */
typedef struct Escopo {
    struct Escopo *pai;
    Simbolo      **simbolos;
    int            n_simbolos;
    int            capacidade;
    char          *prefixo;      /* Prefixo de namespace (ex: "math") */
    int            profundidade_loop;
    TipoIluluC    *tipo_retorno; /* Tipo de retorno da função atual  */
    int            em_funcao;
} Escopo;

/* ===== Tabela de símbolos ===== */
typedef struct TabelaSimbolos {
    Escopo        *global;
    Escopo        *atual;
    ContextoErros *erros;
} TabelaSimbolos;

/* ---------- Ciclo de vida ---------- */

TabelaSimbolos *tabela_criar(ContextoErros *erros);
void            tabela_destruir(TabelaSimbolos *ts);

/* ---------- Gerenciamento de escopos ---------- */

void tabela_entrar_escopo(TabelaSimbolos *ts);
void tabela_sair_escopo(TabelaSimbolos *ts);
void tabela_entrar_funcao(TabelaSimbolos *ts, TipoIluluC *tipo_ret);
void tabela_sair_funcao(TabelaSimbolos *ts);
void tabela_entrar_loop(TabelaSimbolos *ts);
void tabela_sair_loop(TabelaSimbolos *ts);

/* ---------- Registro de símbolos ---------- */

/*
 * Insere um símbolo no escopo atual.
 * Retorna o símbolo criado ou NULL se houver redeclaração.
 */
Simbolo *tabela_inserir(TabelaSimbolos *ts,
                        const char *nome,
                        CategoriaSimbolo cat,
                        TipoIluluC *tipo,
                        No *no_decl);

/* Insere no escopo global (ignora escopo atual) */
Simbolo *tabela_inserir_global(TabelaSimbolos *ts,
                               const char *nome,
                               CategoriaSimbolo cat,
                               TipoIluluC *tipo,
                               No *no_decl);

/* ---------- Consulta ---------- */

/* Busca símbolo subindo a cadeia de escopos */
Simbolo *tabela_buscar(TabelaSimbolos *ts, const char *nome);

/* Busca apenas no escopo global (para tipos nomeados) */
Simbolo *tabela_buscar_global(TabelaSimbolos *ts, const char *nome);

/* Busca struct/enum/alias pelo nome (sempre global) */
Simbolo *tabela_buscar_tipo(TabelaSimbolos *ts, const char *nome);

/* Verifica se estamos dentro de um loop */
int tabela_dentro_loop(const TabelaSimbolos *ts);

/* Retorna o tipo de retorno da função atual (NULL = void) */
TipoIluluC *tabela_tipo_retorno(const TabelaSimbolos *ts);

/* Qualifica nome com prefixo de namespace do escopo */
char *tabela_qualificar_nome(const TabelaSimbolos *ts, const char *nome);

#endif /* ILULUC_SYMBOL_TABLE_H */
