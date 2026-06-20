#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ContextoErros *erros_criar(void) {
    ContextoErros *ctx = calloc(1, sizeof(ContextoErros));
    if (!ctx) {
        return NULL;
    }

    ctx->capacidade = ERROS_CAP_INICIAL;
    ctx->erros = calloc((size_t)ctx->capacidade, sizeof(Erro));
    if (!ctx->erros) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

void erros_destruir(ContextoErros *ctx) {
    if (!ctx) {
        return;
    }
    free(ctx->erros);
    free(ctx);
}

static void erros_garantir_capacidade(ContextoErros *ctx) {
    if (!ctx || ctx->quantidade < ctx->capacidade) {
        return;
    }

    int nova_capacidade = ctx->capacidade > 0 ? ctx->capacidade * 2 : ERROS_CAP_INICIAL;
    Erro *novos = realloc(ctx->erros, (size_t)nova_capacidade * sizeof(Erro));
    if (!novos) {
        ctx->tem_fatal = 1;
        return;
    }

    ctx->erros = novos;
    ctx->capacidade = nova_capacidade;
}

void erros_adicionar(ContextoErros *ctx,
                     TipoErro tipo,
                     Localizacao loc,
                     const char *fmt, ...) {
    va_list args;

    if (!ctx) {
        return;
    }

    erros_garantir_capacidade(ctx);
    if (ctx->quantidade >= ctx->capacidade) {
        ctx->tem_fatal = 1;
        return;
    }

    Erro *erro = &ctx->erros[ctx->quantidade++];
    erro->tipo = tipo;
    erro->loc = loc;

    va_start(args, fmt);
    vsnprintf(erro->mensagem, sizeof(erro->mensagem), fmt, args);
    va_end(args);

    if (tipo == ERRO_INTERNO) {
        ctx->tem_fatal = 1;
    }
}

void erros_imprimir(const ContextoErros *ctx) {
    if (!ctx) {
        return;
    }

    for (int i = 0; i < ctx->quantidade; i++) {
        const Erro *erro = &ctx->erros[i];
        const char *arquivo = erro->loc.arquivo ? erro->loc.arquivo : "<desconhecido>";
        fprintf(stderr,
                "%s:%d:%d: %s: %s\n",
                arquivo,
                erro->loc.linha,
                erro->loc.coluna,
                erro_tipo_nome(erro->tipo),
                erro->mensagem);
    }
}

int erros_tem_erros(const ContextoErros *ctx) {
    return ctx && ctx->quantidade > 0;
}

Localizacao loc_criar(const char *arquivo, int linha, int coluna) {
    Localizacao loc;
    loc.arquivo = arquivo;
    loc.linha = linha;
    loc.coluna = coluna;
    return loc;
}

const char *erro_tipo_nome(TipoErro tipo) {
    switch (tipo) {
        case ERRO_LEXICO:
            return "erro lexico";
        case ERRO_SINTATICO:
            return "erro sintatico";
        case ERRO_SEMANTICO:
            return "erro semantico";
        case ERRO_TIPO:
            return "erro de tipo";
        case ERRO_INTERNO:
            return "erro interno";
        default:
            return "erro";
    }
}
