#include "symbol_table.h"

#include <stdlib.h>
#include <string.h>

static char *sym_strdup(const char *texto) {
    size_t len = strlen(texto) + 1;
    char *copia = malloc(len);
    if (!copia) {
        return NULL;
    }
    memcpy(copia, texto, len);
    return copia;
}

static Escopo *escopo_criar(Escopo *pai) {
    Escopo *escopo = calloc(1, sizeof(Escopo));
    if (!escopo) {
        return NULL;
    }

    escopo->pai = pai;
    escopo->capacidade = 8;
    escopo->simbolos = calloc((size_t)escopo->capacidade, sizeof(Simbolo *));
    if (!escopo->simbolos) {
        free(escopo);
        return NULL;
    }

    if (pai) {
        escopo->prefixo = pai->prefixo ? sym_strdup(pai->prefixo) : NULL;
        escopo->profundidade_loop = pai->profundidade_loop;
        escopo->tipo_retorno = pai->tipo_retorno;
        escopo->em_funcao = pai->em_funcao;
    }

    return escopo;
}

static void simbolo_destruir(Simbolo *simbolo) {
    if (!simbolo) {
        return;
    }

    free(simbolo->nome);
    free(simbolo->nome_curto);
    tipo_liberar_recursivo(simbolo->tipo);
    tipo_liberar_recursivo(simbolo->assinatura.tipo_ret);
    if (simbolo->assinatura.tipos_params) {
        for (int i = 0; i < simbolo->assinatura.n_params; i++) {
            tipo_liberar_recursivo(simbolo->assinatura.tipos_params[i]);
        }
    }
    free(simbolo->assinatura.tipos_params);
    free(simbolo);
}

static void escopo_destruir(Escopo *escopo) {
    if (!escopo) {
        return;
    }

    for (int i = 0; i < escopo->n_simbolos; i++) {
        simbolo_destruir(escopo->simbolos[i]);
    }

    free(escopo->simbolos);
    free(escopo->prefixo);
    free(escopo);
}

static Simbolo *escopo_buscar_local(Escopo *escopo, const char *nome) {
    if (!escopo) {
        return NULL;
    }

    for (int i = 0; i < escopo->n_simbolos; i++) {
        Simbolo *simbolo = escopo->simbolos[i];
        if ((simbolo->nome && strcmp(simbolo->nome, nome) == 0)
            || (simbolo->nome_curto && strcmp(simbolo->nome_curto, nome) == 0)) {
            return simbolo;
        }
    }

    return NULL;
}

static void escopo_garantir_capacidade(Escopo *escopo) {
    if (!escopo || escopo->n_simbolos < escopo->capacidade) {
        return;
    }

    escopo->capacidade *= 2;
    escopo->simbolos = realloc(escopo->simbolos,
                               (size_t)escopo->capacidade * sizeof(Simbolo *));
}

TabelaSimbolos *tabela_criar(ContextoErros *erros) {
    TabelaSimbolos *ts = calloc(1, sizeof(TabelaSimbolos));
    if (!ts) {
        return NULL;
    }

    ts->global = escopo_criar(NULL);
    if (!ts->global) {
        free(ts);
        return NULL;
    }

    ts->atual = ts->global;
    ts->erros = erros;
    return ts;
}

void tabela_destruir(TabelaSimbolos *ts) {
    if (!ts) {
        return;
    }

    while (ts->atual && ts->atual != ts->global) {
        Escopo *pai = ts->atual->pai;
        escopo_destruir(ts->atual);
        ts->atual = pai;
    }

    escopo_destruir(ts->global);
    free(ts);
}

void tabela_entrar_escopo(TabelaSimbolos *ts) {
    Escopo *novo;

    if (!ts) {
        return;
    }

    novo = escopo_criar(ts->atual);
    if (!novo) {
        return;
    }
    ts->atual = novo;
}

void tabela_sair_escopo(TabelaSimbolos *ts) {
    Escopo *atual;

    if (!ts || !ts->atual || ts->atual == ts->global) {
        return;
    }

    atual = ts->atual;
    ts->atual = atual->pai;
    escopo_destruir(atual);
}

void tabela_entrar_funcao(TabelaSimbolos *ts, TipoIluluC *tipo_ret) {
    tabela_entrar_escopo(ts);
    if (ts && ts->atual) {
        ts->atual->em_funcao = 1;
        ts->atual->tipo_retorno = tipo_ret;
        ts->atual->profundidade_loop = 0;
    }
}

void tabela_sair_funcao(TabelaSimbolos *ts) {
    tabela_sair_escopo(ts);
}

void tabela_entrar_loop(TabelaSimbolos *ts) {
    if (ts && ts->atual) {
        ts->atual->profundidade_loop++;
    }
}

void tabela_sair_loop(TabelaSimbolos *ts) {
    if (ts && ts->atual && ts->atual->profundidade_loop > 0) {
        ts->atual->profundidade_loop--;
    }
}

char *tabela_qualificar_nome(const TabelaSimbolos *ts, const char *nome) {
    size_t prefixo_len;
    size_t nome_len;
    char *qualificado;

    if (!nome) {
        return NULL;
    }

    if (!ts || !ts->atual || !ts->atual->prefixo || ts->atual->prefixo[0] == '\0') {
        return sym_strdup(nome);
    }

    prefixo_len = strlen(ts->atual->prefixo);
    nome_len = strlen(nome);
    qualificado = malloc(prefixo_len + 2 + nome_len + 1);
    if (!qualificado) {
        return NULL;
    }

    memcpy(qualificado, ts->atual->prefixo, prefixo_len);
    memcpy(qualificado + prefixo_len, "::", 2);
    memcpy(qualificado + prefixo_len + 2, nome, nome_len + 1);
    return qualificado;
}

Simbolo *tabela_inserir(TabelaSimbolos *ts,
                        const char *nome,
                        CategoriaSimbolo cat,
                        TipoIluluC *tipo,
                        No *no_decl) {
    Simbolo *existente;
    Simbolo *simbolo;
    char *qualificado;

    if (!ts || !ts->atual || !nome) {
        return NULL;
    }

    existente = escopo_buscar_local(ts->atual, nome);
    if (existente) {
        if (ts->erros && no_decl) {
            erros_adicionar(ts->erros,
                            ERRO_SEMANTICO,
                            no_decl->loc,
                            "Simbolo '%s' ja declarado neste escopo",
                            nome);
        }
        return NULL;
    }

    simbolo = calloc(1, sizeof(Simbolo));
    if (!simbolo) {
        return NULL;
    }

    qualificado = tabela_qualificar_nome(ts, nome);
    simbolo->nome = qualificado ? qualificado : sym_strdup(nome);
    simbolo->nome_curto = sym_strdup(nome);
    simbolo->categoria = cat;
    simbolo->tipo = tipo;
    simbolo->no_decl = no_decl;
    simbolo->definido = 1;

    escopo_garantir_capacidade(ts->atual);
    ts->atual->simbolos[ts->atual->n_simbolos++] = simbolo;
    return simbolo;
}

Simbolo *tabela_inserir_global(TabelaSimbolos *ts,
                               const char *nome,
                               CategoriaSimbolo cat,
                               TipoIluluC *tipo,
                               No *no_decl) {
    Escopo *salvo;
    Simbolo *simbolo;

    if (!ts) {
        return NULL;
    }

    salvo = ts->atual;
    ts->atual = ts->global;
    simbolo = tabela_inserir(ts, nome, cat, tipo, no_decl);
    ts->atual = salvo;
    return simbolo;
}

Simbolo *tabela_buscar(TabelaSimbolos *ts, const char *nome) {
    Escopo *escopo;

    if (!ts || !nome) {
        return NULL;
    }

    for (escopo = ts->atual; escopo; escopo = escopo->pai) {
        Simbolo *simbolo = escopo_buscar_local(escopo, nome);
        if (simbolo) {
            return simbolo;
        }
    }

    return NULL;
}

Simbolo *tabela_buscar_global(TabelaSimbolos *ts, const char *nome) {
    if (!ts || !ts->global || !nome) {
        return NULL;
    }
    return escopo_buscar_local(ts->global, nome);
}

Simbolo *tabela_buscar_tipo(TabelaSimbolos *ts, const char *nome) {
    Simbolo *simbolo = tabela_buscar_global(ts, nome);

    if (!simbolo) {
        return NULL;
    }

    switch (simbolo->categoria) {
        case SIMB_STRUCT:
        case SIMB_ENUM:
        case SIMB_ALIAS:
            return simbolo;
        default:
            return NULL;
    }
}

int tabela_dentro_loop(const TabelaSimbolos *ts) {
    return ts && ts->atual && ts->atual->profundidade_loop > 0;
}

TipoIluluC *tabela_tipo_retorno(const TabelaSimbolos *ts) {
    if (!ts || !ts->atual) {
        return NULL;
    }
    return ts->atual->tipo_retorno;
}
