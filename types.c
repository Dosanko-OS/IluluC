#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol_table.h"

static TipoIluluC *singleton_desconhecido;
static TipoIluluC *singleton_void;
static TipoIluluC *singleton_string;
static TipoIluluC *singleton_bool;

static int tipo_eh_singleton(const TipoIluluC *t) {
    return t == singleton_desconhecido
        || t == singleton_void
        || t == singleton_string
        || t == singleton_bool;
}

static char *types_strdup(const char *texto) {
    size_t len = strlen(texto) + 1;
    char *copia = malloc(len);
    if (!copia) {
        return NULL;
    }
    memcpy(copia, texto, len);
    return copia;
}

TipoIluluC *tipo_de_no_ast(const No *no_tipo) {
    TipoIluluC *tipo;

    if (!no_tipo || no_tipo->tipo != NO_TIPO) {
        return tipo_desconhecido();
    }

    tipo = tipo_criar(no_tipo->tipo_no.categoria);
    if (!tipo) {
        return tipo_desconhecido();
    }

    tipo->tamanho = no_tipo->tipo_no.tamanho;
    if (no_tipo->tipo_no.nome) {
        tipo->nome = types_strdup(no_tipo->tipo_no.nome);
    }
    if (no_tipo->tipo_no.subtipo) {
        tipo->subtipo = tipo_de_no_ast(no_tipo->tipo_no.subtipo);
    }
    return tipo;
}

TipoIluluC *tipo_copiar(const TipoIluluC *t) {
    TipoIluluC *copia;

    if (!t) {
        return NULL;
    }
    if (t == tipo_desconhecido()) {
        return tipo_desconhecido();
    }
    if (t == tipo_void()) {
        return tipo_void();
    }
    if (t == tipo_string()) {
        return tipo_string();
    }
    if (t == tipo_bool()) {
        return tipo_bool();
    }

    copia = tipo_criar(t->categoria);
    if (!copia) {
        return tipo_desconhecido();
    }

    copia->tamanho = t->tamanho;
    if (t->nome) {
        copia->nome = types_strdup(t->nome);
    }
    if (t->subtipo) {
        copia->subtipo = tipo_copiar(t->subtipo);
    }
    return copia;
}

void tipo_liberar_recursivo(TipoIluluC *t) {
    if (!t || tipo_eh_singleton(t)) {
        return;
    }
    tipo_liberar_recursivo(t->subtipo);
    free(t->nome);
    free(t);
}

TipoIluluC *tipo_resolver(TipoIluluC *t, struct TabelaSimbolos *ts) {
    Simbolo *simbolo;

    if (!t) {
        return tipo_desconhecido();
    }
    if (!ts) {
        return t;
    }

    if (t->categoria == TIPO_ALIAS && t->nome) {
        simbolo = tabela_buscar_tipo(ts, t->nome);
        if (simbolo && simbolo->tipo) {
            return simbolo->tipo;
        }
    }

    if ((t->categoria == TIPO_STRUCT || t->categoria == TIPO_ENUM || t->categoria == TIPO_GENERICO)
        && t->nome) {
        simbolo = tabela_buscar_tipo(ts, t->nome);
        if (simbolo && simbolo->tipo) {
            return simbolo->tipo;
        }
    }

    return t;
}

int tipo_atribuivel(const TipoIluluC *dest, const TipoIluluC *src) {
    if (!dest || !src) {
        return 0;
    }
    if (dest == tipo_desconhecido() || src == tipo_desconhecido()) {
        return 1;
    }
    if (tipo_igual(dest, src)) {
        return 1;
    }
    if (tipo_eh_numerico(dest) && tipo_eh_numerico(src)) {
        return 1;
    }
    return 0;
}

int tipo_compativel_binario(const TipoIluluC *a, const TipoIluluC *b) {
    if (!a || !b) {
        return 0;
    }
    if (a == tipo_desconhecido() || b == tipo_desconhecido()) {
        return 1;
    }
    if (tipo_eh_numerico(a) && tipo_eh_numerico(b)) {
        return 1;
    }
    return tipo_igual(a, b);
}

TipoIluluC *tipo_promover(const TipoIluluC *a, const TipoIluluC *b) {
    if (!a) {
        return tipo_copiar(b);
    }
    if (!b) {
        return tipo_copiar(a);
    }
    if (a == tipo_desconhecido() || b == tipo_desconhecido()) {
        return tipo_desconhecido();
    }
    if (a->categoria == TIPO_F64 || b->categoria == TIPO_F64) {
        return tipo_criar(TIPO_F64);
    }
    if (a->categoria == TIPO_F32 || b->categoria == TIPO_F32) {
        return tipo_criar(TIPO_F32);
    }
    if (tipo_eh_numerico(a) && tipo_eh_numerico(b)) {
        return tipo_criar(TIPO_I32);
    }
    return tipo_copiar(a);
}

int tipo_eh_verdadeiro(const TipoIluluC *t) {
    if (!t) {
        return 0;
    }
    return t->categoria == TIPO_BOOL
        || tipo_eh_numerico(t)
        || t->categoria == TIPO_PONTEIRO;
}

int tipo_eh_ponteiro_ou_null(const TipoIluluC *t) {
    return t && t->categoria == TIPO_PONTEIRO;
}

const char *tipo_primitivo_c(CategoriaType cat) {
    switch (cat) {
        case TIPO_U8: return "uint8_t";
        case TIPO_U16: return "uint16_t";
        case TIPO_U32: return "uint32_t";
        case TIPO_U64: return "uint64_t";
        case TIPO_I8: return "int8_t";
        case TIPO_I16: return "int16_t";
        case TIPO_I32: return "int32_t";
        case TIPO_I64: return "int64_t";
        case TIPO_F32: return "float";
        case TIPO_F64: return "double";
        case TIPO_BOOL: return "bool";
        case TIPO_CHAR: return "char";
        case TIPO_STRING: return "const char *";
        case TIPO_VOID: return "void";
        default: return NULL;
    }
}

int tipo_formato_c(const TipoIluluC *t,
                   const char *nome_var,
                   char *buf,
                   int buf_sz) {
    const char *nome;
    char subtipo_buf[TIPO_BUF_MAX];

    if (!buf || buf_sz <= 0) {
        return 0;
    }

    if (!t) {
        return snprintf(buf, (size_t)buf_sz, "void%s%s",
                        nome_var ? " " : "",
                        nome_var ? nome_var : "");
    }

    nome = tipo_primitivo_c(t->categoria);
    if (nome) {
        return snprintf(buf, (size_t)buf_sz, "%s%s%s",
                        nome,
                        nome_var ? " " : "",
                        nome_var ? nome_var : "");
    }

    switch (t->categoria) {
        case TIPO_PONTEIRO:
            tipo_formato_c(t->subtipo, NULL, subtipo_buf, sizeof(subtipo_buf));
            return snprintf(buf, (size_t)buf_sz, "%s *%s",
                            subtipo_buf,
                            nome_var ? nome_var : "");
        case TIPO_ARRAY:
            tipo_formato_c(t->subtipo, NULL, subtipo_buf, sizeof(subtipo_buf));
            return snprintf(buf, (size_t)buf_sz, "%s %s[%d]",
                            subtipo_buf,
                            nome_var ? nome_var : "",
                            t->tamanho);
        case TIPO_STRUCT:
            return snprintf(buf, (size_t)buf_sz, "struct %s%s%s",
                            t->nome ? t->nome : "anon",
                            nome_var ? " " : "",
                            nome_var ? nome_var : "");
        case TIPO_ENUM:
            return snprintf(buf, (size_t)buf_sz, "int%s%s",
                            nome_var ? " " : "",
                            nome_var ? nome_var : "");
        case TIPO_ALIAS:
        case TIPO_GENERICO:
            return snprintf(buf, (size_t)buf_sz, "%s%s%s",
                            t->nome ? t->nome : "void",
                            nome_var ? " " : "",
                            nome_var ? nome_var : "");
        case TIPO_DESCONHECIDO:
            return snprintf(buf, (size_t)buf_sz, "int%s%s",
                            nome_var ? " " : "",
                            nome_var ? nome_var : "");
        default:
            return snprintf(buf, (size_t)buf_sz, "void%s%s",
                            nome_var ? " " : "",
                            nome_var ? nome_var : "");
    }
}

TipoIluluC *tipo_desconhecido(void) {
    if (!singleton_desconhecido) {
        singleton_desconhecido = tipo_criar(TIPO_DESCONHECIDO);
    }
    return singleton_desconhecido;
}

TipoIluluC *tipo_void(void) {
    if (!singleton_void) {
        singleton_void = tipo_criar(TIPO_VOID);
    }
    return singleton_void;
}

TipoIluluC *tipo_string(void) {
    if (!singleton_string) {
        singleton_string = tipo_criar(TIPO_STRING);
    }
    return singleton_string;
}

TipoIluluC *tipo_bool(void) {
    if (!singleton_bool) {
        singleton_bool = tipo_criar(TIPO_BOOL);
    }
    return singleton_bool;
}

void tipos_singletons_destruir(void) {
    tipo_destruir(singleton_desconhecido);
    tipo_destruir(singleton_void);
    tipo_destruir(singleton_string);
    tipo_destruir(singleton_bool);
    singleton_desconhecido = NULL;
    singleton_void = NULL;
    singleton_string = NULL;
    singleton_bool = NULL;
}
