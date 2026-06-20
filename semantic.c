#include "semantic.h"

#include <stdlib.h>
#include <string.h>

static char *sem_strdup(const char *texto) {
    size_t len = strlen(texto) + 1;
    char *copia = malloc(len);
    if (!copia) {
        return NULL;
    }
    memcpy(copia, texto, len);
    return copia;
}

static char *sem_prefixo_concat(const char *a, const char *b) {
    size_t a_len;
    size_t b_len;
    char *resultado;

    if (!a || a[0] == '\0') {
        return sem_strdup(b);
    }

    a_len = strlen(a);
    b_len = strlen(b);
    resultado = malloc(a_len + 2 + b_len + 1);
    if (!resultado) {
        return NULL;
    }

    memcpy(resultado, a, a_len);
    memcpy(resultado + a_len, "::", 2);
    memcpy(resultado + a_len + 2, b, b_len + 1);
    return resultado;
}

static void sem_visitar_expr(ContextoSemantico *ctx, No *expr);
static void sem_visitar_stmt(ContextoSemantico *ctx, No *stmt);
static void sem_visitar_decl(ContextoSemantico *ctx, No *decl);

static TipoIluluC *sem_tipo_decl(No *no_tipo) {
    if (!no_tipo) {
        return tipo_void();
    }
    return tipo_de_no_ast(no_tipo);
}

static void sem_preencher_assinatura(Simbolo *simbolo, No **params, int n_params, No *tipo_ret) {
    if (!simbolo) {
        return;
    }

    simbolo->assinatura.n_params = n_params;
    simbolo->assinatura.tipo_ret = tipo_copiar(sem_tipo_decl(tipo_ret));
    if (n_params <= 0) {
        simbolo->assinatura.tipos_params = NULL;
        return;
    }

    simbolo->assinatura.tipos_params = calloc((size_t)n_params, sizeof(TipoIluluC *));
    if (!simbolo->assinatura.tipos_params) {
        simbolo->assinatura.n_params = 0;
        return;
    }

    for (int i = 0; i < n_params; i++) {
        simbolo->assinatura.tipos_params[i] = tipo_de_no_ast(params[i]->param.tipo);
    }
}

static void sem_registrar_var(ContextoSemantico *ctx, No *decl, CategoriaSimbolo categoria) {
    TipoIluluC *tipo = decl->var.tipo ? tipo_de_no_ast(decl->var.tipo) : tipo_desconhecido();
    tabela_inserir(ctx->tabela, decl->var.nome, categoria, tipo, decl);
    if (decl->var.valor) {
        sem_visitar_expr(ctx, decl->var.valor);
    }
}

static void sem_visitar_bloco(ContextoSemantico *ctx, No *bloco) {
    if (!bloco) {
        return;
    }

    tabela_entrar_escopo(ctx->tabela);
    for (int i = 0; i < bloco->bloco.n_stmts; i++) {
        sem_visitar_stmt(ctx, bloco->bloco.stmts[i]);
    }
    tabela_sair_escopo(ctx->tabela);
}

static void sem_visitar_expr(ContextoSemantico *ctx, No *expr) {
    if (!ctx || !expr) {
        return;
    }

    switch (expr->tipo) {
        case NO_BINARIO:
            sem_visitar_expr(ctx, expr->binario.esq);
            sem_visitar_expr(ctx, expr->binario.dir);
            break;
        case NO_UNARIO:
            sem_visitar_expr(ctx, expr->unario.operando);
            break;
        case NO_CHAMADA:
            sem_visitar_expr(ctx, expr->chamada.funcao);
            for (int i = 0; i < expr->chamada.n_args; i++) {
                sem_visitar_expr(ctx, expr->chamada.args[i]);
            }
            break;
        case NO_INDEX:
            sem_visitar_expr(ctx, expr->index.array);
            sem_visitar_expr(ctx, expr->index.indice);
            break;
        case NO_CAMPO_ACESSO:
            sem_visitar_expr(ctx, expr->campo_acesso.objeto);
            break;
        case NO_CAST:
            sem_visitar_expr(ctx, expr->cast.expr);
            break;
        case NO_ADDRESS:
        case NO_DEREF:
            sem_visitar_expr(ctx, expr->ref.expr);
            break;
        default:
            break;
    }
}

static void sem_visitar_stmt(ContextoSemantico *ctx, No *stmt) {
    if (!ctx || !stmt) {
        return;
    }

    switch (stmt->tipo) {
        case NO_VAR_DECL:
            sem_registrar_var(ctx, stmt, SIMB_VAR);
            break;
        case NO_CONST_DECL:
            tabela_inserir(ctx->tabela,
                           stmt->constante.nome,
                           SIMB_CONST,
                           tipo_desconhecido(),
                           stmt);
            sem_visitar_expr(ctx, stmt->constante.valor);
            break;
        case NO_BLOCO:
            sem_visitar_bloco(ctx, stmt);
            break;
        case NO_IF:
            sem_visitar_expr(ctx, stmt->se.cond);
            sem_visitar_stmt(ctx, stmt->se.entao);
            sem_visitar_stmt(ctx, stmt->se.senao);
            break;
        case NO_WHILE:
            sem_visitar_expr(ctx, stmt->enquanto.cond);
            tabela_entrar_loop(ctx->tabela);
            sem_visitar_stmt(ctx, stmt->enquanto.corpo);
            tabela_sair_loop(ctx->tabela);
            break;
        case NO_LOOP_FOREVER:
            tabela_entrar_loop(ctx->tabela);
            sem_visitar_stmt(ctx, stmt->loop_eterno.corpo);
            tabela_sair_loop(ctx->tabela);
            break;
        case NO_LOOP_FOR: {
            TipoIluluC *tipo_indice = tipo_criar(TIPO_I32);
            sem_visitar_expr(ctx, stmt->loop_for.inicio);
            sem_visitar_expr(ctx, stmt->loop_for.fim);
            tabela_entrar_escopo(ctx->tabela);
            tabela_entrar_loop(ctx->tabela);
            tabela_inserir(ctx->tabela, stmt->loop_for.var, SIMB_VAR, tipo_indice, stmt);
            sem_visitar_stmt(ctx, stmt->loop_for.corpo);
            tabela_sair_loop(ctx->tabela);
            tabela_sair_escopo(ctx->tabela);
            break;
        }
        case NO_RETURN:
            sem_visitar_expr(ctx, stmt->retorno.valor);
            break;
        case NO_MATCH:
            sem_visitar_expr(ctx, stmt->match.expr);
            for (int i = 0; i < stmt->match.n_bracos; i++) {
                sem_visitar_stmt(ctx, stmt->match.bracos[i]->braco.corpo);
            }
            break;
        case NO_UNSAFE:
            sem_visitar_stmt(ctx, stmt->bloco_unsafe.bloco);
            break;
        case NO_EXPR_STMT:
            sem_visitar_expr(ctx, stmt->expr_stmt.expr);
            break;
        default:
            break;
    }
}

static void sem_visitar_funcao(ContextoSemantico *ctx, No *decl, CategoriaSimbolo categoria) {
    TipoIluluC *tipo_ret = sem_tipo_decl(decl->funcao.tipo_ret);
    Simbolo *simbolo = tabela_inserir_global(ctx->tabela,
                                             decl->funcao.nome,
                                             categoria,
                                             tipo_copiar(tipo_ret),
                                             decl);
    if (simbolo) {
        sem_preencher_assinatura(simbolo,
                                 decl->funcao.params,
                                 decl->funcao.n_params,
                                 decl->funcao.tipo_ret);
        if (categoria == SIMB_FUNCAO && strcmp(decl->funcao.nome, "main") == 0) {
            ctx->tem_main = 1;
        }
    }

    if (categoria == SIMB_EXTERN || !decl->funcao.corpo) {
        return;
    }

    tabela_entrar_funcao(ctx->tabela, tipo_ret);
    for (int i = 0; i < decl->funcao.n_params; i++) {
        No *param = decl->funcao.params[i];
        tabela_inserir(ctx->tabela,
                       param->param.nome,
                       SIMB_PARAM,
                       tipo_de_no_ast(param->param.tipo),
                       param);
    }
    sem_visitar_stmt(ctx, decl->funcao.corpo);
    tabela_sair_funcao(ctx->tabela);
}

static void sem_visitar_decl(ContextoSemantico *ctx, No *decl) {
    char *prefixo_anterior;

    if (!ctx || !decl) {
        return;
    }

    switch (decl->tipo) {
        case NO_FUNCAO_DECL:
            sem_visitar_funcao(ctx, decl, SIMB_FUNCAO);
            break;
        case NO_EXTERN_DECL: {
            No falso = {0};
            falso.tipo = NO_FUNCAO_DECL;
            falso.loc = decl->loc;
            falso.funcao.nome = decl->externo.nome;
            falso.funcao.params = decl->externo.params;
            falso.funcao.n_params = decl->externo.n_params;
            falso.funcao.tipo_ret = decl->externo.tipo_ret;
            falso.funcao.corpo = NULL;
            sem_visitar_funcao(ctx, &falso, SIMB_EXTERN);
            break;
        }
        case NO_STRUCT_DECL: {
            TipoIluluC *tipo = tipo_criar(TIPO_STRUCT);
            if (tipo) {
                tipo->nome = sem_strdup(decl->estrutura.nome);
            }
            tabela_inserir_global(ctx->tabela, decl->estrutura.nome, SIMB_STRUCT, tipo, decl);
            break;
        }
        case NO_ENUM_DECL: {
            TipoIluluC *tipo = tipo_criar(TIPO_ENUM);
            if (tipo) {
                tipo->nome = sem_strdup(decl->enumeracao.nome);
            }
            tabela_inserir_global(ctx->tabela, decl->enumeracao.nome, SIMB_ENUM, tipo, decl);
            break;
        }
        case NO_ALIAS_TIPO:
            tabela_inserir_global(ctx->tabela,
                                  decl->alias.nome,
                                  SIMB_ALIAS,
                                  tipo_de_no_ast(decl->alias.tipo),
                                  decl);
            break;
        case NO_CONST_DECL:
            tabela_inserir_global(ctx->tabela,
                                  decl->constante.nome,
                                  SIMB_CONST,
                                  tipo_desconhecido(),
                                  decl);
            sem_visitar_expr(ctx, decl->constante.valor);
            break;
        case NO_NAMESPACE:
            tabela_inserir_global(ctx->tabela,
                                  decl->espaco.nome,
                                  SIMB_NAMESPACE,
                                  NULL,
                                  decl);
            prefixo_anterior = ctx->tabela->global->prefixo;
            ctx->tabela->global->prefixo =
                sem_prefixo_concat(prefixo_anterior, decl->espaco.nome);
            for (int i = 0; i < decl->espaco.n_itens; i++) {
                sem_visitar_decl(ctx, decl->espaco.itens[i]);
            }
            free(ctx->tabela->global->prefixo);
            ctx->tabela->global->prefixo = prefixo_anterior;
            break;
        case NO_USE_DECL:
            break;
        default:
            break;
    }
}

ContextoSemantico *semantica_analisar(No *programa, ContextoErros *erros) {
    ContextoSemantico *ctx;

    ctx = calloc(1, sizeof(ContextoSemantico));
    if (!ctx) {
        return NULL;
    }

    ctx->tabela = tabela_criar(erros);
    ctx->erros = erros;
    if (!ctx->tabela) {
        free(ctx);
        return NULL;
    }

    if (programa && programa->tipo == NO_PROGRAMA) {
        for (int i = 0; i < programa->programa.n_decls; i++) {
            sem_visitar_decl(ctx, programa->programa.decls[i]);
        }
    }

    if (!ctx->tem_main) {
        erros_adicionar(erros,
                        ERRO_SEMANTICO,
                        programa ? programa->loc : loc_criar(NULL, 0, 0),
                        "Programa nao define a funcao 'main'");
    }

    ctx->tem_erros = erros_tem_erros(erros);
    return ctx;
}

void semantica_destruir(ContextoSemantico *ctx) {
    if (!ctx) {
        return;
    }
    tabela_destruir(ctx->tabela);
    free(ctx);
}
