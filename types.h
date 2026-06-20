/*
 * types.h - Utilitários do sistema de tipos IluluC
 *
 * Complementa TipoIluluC (definido em ast.h) com conversão, resolução
 * de aliases, compatibilidade e formatação para código C gerado.
 */

#ifndef ILULUC_TYPES_H
#define ILULUC_TYPES_H

#include "ast.h"

/* Declaração antecipada */
struct TabelaSimbolos;

/* ===== Informação de função (para verificação de chamadas) ===== */
typedef struct {
    TipoIluluC **tipos_params;
    int          n_params;
    TipoIluluC  *tipo_ret;
} AssinaturaFuncao;

/* ---------- Conversão AST → TipoIluluC ---------- */

/* Converte um nó NO_TIPO da AST em TipoIluluC (sem resolver aliases) */
TipoIluluC *tipo_de_no_ast(const No *no_tipo);

/* Copia recursivamente um TipoIluluC */
TipoIluluC *tipo_copiar(const TipoIluluC *t);

/* Libera um tipo e todos os seus subtipos */
void tipo_liberar_recursivo(TipoIluluC *t);

/* ---------- Resolução e comparação ---------- */

/* Resolve aliases e tipos nomeados usando a tabela de símbolos */
TipoIluluC *tipo_resolver(TipoIluluC *t, struct TabelaSimbolos *ts);

/* Verifica se src pode ser atribuído a dest */
int tipo_atribuivel(const TipoIluluC *dest, const TipoIluluC *src);

/* Verifica compatibilidade em operações aritméticas/binárias */
int tipo_compativel_binario(const TipoIluluC *a, const TipoIluluC *b);

/* Retorna o tipo comum promovido para operação aritmética */
TipoIluluC *tipo_promover(const TipoIluluC *a, const TipoIluluC *b);

/* Verifica se o tipo é bool ou numérico (para condições) */
int tipo_eh_verdadeiro(const TipoIluluC *t);

/* Verifica se o tipo é ponteiro ou null-compatível */
int tipo_eh_ponteiro_ou_null(const TipoIluluC *t);

/* ---------- Formatação para C ---------- */

/* Tamanho do buffer recomendado para tipo_formato_c */
#define TIPO_BUF_MAX 256

/*
 * Escreve a representação C completa do tipo em buf.
 * Para arrays e ponteiros, nome_var pode ser inserido no meio:
 *   tipo_formato_c(t, "x", buf, sz) → "uint8_t x[10]" ou "uint8_t* x"
 * Se nome_var for NULL, retorna apenas o tipo base.
 */
int tipo_formato_c(const TipoIluluC *t,
                   const char *nome_var,
                   char *buf,
                   int buf_sz);

/* Retorna o nome C de um tipo primitivo (sem ponteiros/arrays) */
const char *tipo_primitivo_c(CategoriaType cat);

/* ---------- Tipos especiais ---------- */

/* Tipo desconhecido singleton (para recuperação de erros) */
TipoIluluC *tipo_desconhecido(void);

/* Tipo void singleton */
TipoIluluC *tipo_void(void);

/* Tipo string singleton */
TipoIluluC *tipo_string(void);

/* Tipo bool singleton */
TipoIluluC *tipo_bool(void);

/* Libera singletons alocados (chamar no fim do compilador) */
void tipos_singletons_destruir(void);

#endif /* ILULUC_TYPES_H */
