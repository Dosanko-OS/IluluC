#include "type_checker.h"

#include <string.h>

#include "symbol_table.h"
#include "types.h"

static TipoIluluC *tc_checar_expr(No *expr, ContextoSemantico *ctx);
static void tc_checar_stmt(No *stmt, ContextoSemantico *ctx);
static void tc_checar_decl(No *decl, ContextoSemantico *ctx);

static TipoIluluC *tc_tipo_expr(No *expr) {
    return expr && expr->tipo_inferido ? expr->tipo_inferido : tipo_desconhecido();
}

static void tc_reportar(ContextoSemantico *ctx, No *no, const char *mensagem) {
    if (!ctx || !ctx->erros || !no) {
        return;
    }
    erros_adicionar(ctx->erros, ERRO_TIPO, no->loc, "%s", mensagem);
}

static TipoIluluC *tc_tipo_struct_campo(ContextoSemantico *ctx,
                                        const TipoIluluC *tipo_obj,
                                        const char *campo,
                                        No *no) {
    Simbolo *simbolo;

    if (!tipo_obj || tipo_obj->categoria != TIPO_STRUCT || !tipo_obj->nome) {
        tc_reportar(ctx, no, "Acesso de campo exige valor do tipo struct");
        return tipo_desconhecido();
    }

    simbolo = tabela_buscar_tipo(ctx->tabela, tipo_obj->nome);
    if (!simbolo || !simbolo->no_decl || simbolo->no_decl->tipo != NO_STRUCT_DECL) {
        tc_reportar(ctx, no, "Struct referenciada nao foi encontrada");
        return tipo_desconhecido();
    }

    for (int i = 0; i < simbolo->no_decl->estrutura.n_campos; i++) {
        No *campo_decl = simbolo->no_decl->estrutura.campos[i];
        if (strcmp(campo_decl->campo.nome, campo) == 0) {
            return tipo_de_no_ast(campo_decl->campo.tipo);
        }
    }

    tc_reportar(ctx, no, "Campo nao existe na struct");
    return tipo_desconhecido();
}

static TipoIluluC *tc_checar_chamada(No *expr, ContextoSemantico *ctx) {
    No *funcao = expr->chamada.funcao;

    for (int i = 0; i < expr->chamada.n_args; i++) {
        tc_checar_expr(expr->chamada.args[i], ctx);
    }

    if (funcao && funcao->tipo == NO_IDENT && strcmp(funcao->ident.nome, "print") == 0) {
        expr->tipo_inferido = tipo_void();
        return expr->tipo_inferido;
    }

    if (funcao && funcao->tipo == NO_IDENT) {
        Simbolo *simbolo = tabela_buscar(ctx->tabela, funcao->ident.nome);
        if (!simbolo || (simbolo->categoria != SIMB_FUNCAO && simbolo->categoria != SIMB_EXTERN)) {
            tc_reportar(ctx, expr, "Chamada para funcao nao declarada");
            expr->tipo_inferido = tipo_desconhecido();
            return expr->tipo_inferido;
        }

        if (simbolo->assinatura.n_params != expr->chamada.n_args) {
            tc_reportar(ctx, expr, "Quantidade de argumentos invalida");
        } else {
            for (int i = 0; i < expr->chamada.n_args; i++) {
                TipoIluluC *arg = tc_tipo_expr(expr->chamada.args[i]);
                TipoIluluC *param = simbolo->assinatura.tipos_params[i];
                if (!tipo_atribuivel(param, arg)) {
                    tc_reportar(ctx, expr->chamada.args[i], "Argumento com tipo incompativel");
                }
            }
        }

        expr->tipo_inferido = simbolo->assinatura.tipo_ret
            ? tipo_copiar(simbolo->assinatura.tipo_ret)
            : tipo_void();
        return expr->tipo_inferido;
    }

    tc_checar_expr(funcao, ctx);
    expr->tipo_inferido = tipo_desconhecido();
    return expr->tipo_inferido;
}

static TipoIluluC *tc_checar_expr(No *expr, ContextoSemantico *ctx) {
    TipoIluluC *esq;
    TipoIluluC *dir;

    if (!expr) {
        return tipo_void();
    }

    switch (expr->tipo) {
        case NO_LIT_INT:
            expr->tipo_inferido = tipo_criar(TIPO_I32);
            break;
        case NO_LIT_FLOAT:
            expr->tipo_inferido = tipo_criar(TIPO_F64);
            break;
        case NO_LIT_STRING:
            expr->tipo_inferido = tipo_string();
            break;
        case NO_LIT_CHAR:
            expr->tipo_inferido = tipo_criar(TIPO_CHAR);
            break;
        case NO_LIT_BOOL:
            expr->tipo_inferido = tipo_bool();
            break;
        case NO_LIT_NULL:
            expr->tipo_inferido = tipo_desconhecido();
            break;
        case NO_IDENT: {
            Simbolo *simbolo = tabela_buscar(ctx->tabela, expr->ident.nome);
            if (!simbolo) {
                tc_reportar(ctx, expr, "Identificador nao declarado");
                expr->tipo_inferido = tipo_desconhecido();
            } else {
                expr->tipo_inferido = simbolo->tipo ? tipo_copiar(simbolo->tipo) : tipo_desconhecido();
            }
            break;
        }
        case NO_UNARIO:
            esq = tc_checar_expr(expr->unario.operando, ctx);
            if (expr->unario.op == TOKEN_EXCLAMACAO) {
                if (!tipo_eh_verdadeiro(esq)) {
                    tc_reportar(ctx, expr, "Operador '!' exige expressao booleana ou condicional");
                }
                expr->tipo_inferido = tipo_bool();
            } else {
                if (!tipo_eh_numerico(esq)) {
                    tc_reportar(ctx, expr, "Operador unario exige operando numerico");
                }
                expr->tipo_inferido = tipo_copiar(esq);
            }
            break;
        case NO_BINARIO:
            esq = tc_checar_expr(expr->binario.esq, ctx);
            dir = tc_checar_expr(expr->binario.dir, ctx);
            switch (expr->binario.op) {
                case TOKEN_IGUAL:
                    if (!tipo_atribuivel(esq, dir)) {
                        tc_reportar(ctx, expr, "Atribuicao com tipos incompativeis");
                    }
                    expr->tipo_inferido = tipo_copiar(esq);
                    break;
                case TOKEN_MAIS:
                case TOKEN_MENOS:
                case TOKEN_STAR:
                case TOKEN_BARRA:
                case TOKEN_PORCENTO:
                    if (!tipo_compativel_binario(esq, dir)) {
                        tc_reportar(ctx, expr, "Operacao aritmetica com tipos incompativeis");
                    }
                    expr->tipo_inferido = tipo_promover(esq, dir);
                    break;
                case TOKEN_E_E:
                case TOKEN_OU_OU:
                    if (!tipo_eh_verdadeiro(esq) || !tipo_eh_verdadeiro(dir)) {
                        tc_reportar(ctx, expr, "Operacao logica exige expressoes condicionais");
                    }
                    expr->tipo_inferido = tipo_bool();
                    break;
                default:
                    if (!tipo_compativel_binario(esq, dir)) {
                        tc_reportar(ctx, expr, "Comparacao com tipos incompativeis");
                    }
                    expr->tipo_inferido = tipo_bool();
                    break;
            }
            break;
        case NO_CHAMADA:
            return tc_checar_chamada(expr, ctx);
        case NO_INDEX:
            esq = tc_checar_expr(expr->index.array, ctx);
            dir = tc_checar_expr(expr->index.indice, ctx);
            if (!tipo_eh_inteiro(dir)) {
                tc_reportar(ctx, expr->index.indice, "Indice precisa ser inteiro");
            }
            if (esq->categoria == TIPO_ARRAY || esq->categoria == TIPO_PONTEIRO) {
                expr->tipo_inferido = esq->subtipo ? tipo_copiar(esq->subtipo) : tipo_desconhecido();
            } else {
                tc_reportar(ctx, expr, "Indexacao exige array ou ponteiro");
                expr->tipo_inferido = tipo_desconhecido();
            }
            break;
        case NO_CAMPO_ACESSO:
            esq = tc_checar_expr(expr->campo_acesso.objeto, ctx);
            expr->tipo_inferido = tc_tipo_struct_campo(ctx, esq, expr->campo_acesso.campo, expr);
            break;
        case NO_CAST:
            tc_checar_expr(expr->cast.expr, ctx);
            expr->tipo_inferido = tipo_de_no_ast(expr->cast.tipo);
            break;
        case NO_ADDRESS:
            esq = tc_checar_expr(expr->ref.expr, ctx);
            expr->tipo_inferido = tipo_ponteiro(tipo_copiar(esq));
            break;
        case NO_DEREF:
            esq = tc_checar_expr(expr->ref.expr, ctx);
            if (esq->categoria != TIPO_PONTEIRO || !esq->subtipo) {
                tc_reportar(ctx, expr, "deref exige expressao ponteiro");
                expr->tipo_inferido = tipo_desconhecido();
            } else {
                expr->tipo_inferido = tipo_copiar(esq->subtipo);
            }
            break;
        default:
            expr->tipo_inferido = tipo_desconhecido();
            break;
    }

    return expr->tipo_inferido ? expr->tipo_inferido : tipo_desconhecido();
}

static void tc_checar_bloco(No *bloco, ContextoSemantico *ctx) {
    if (!bloco) {
        return;
    }

    tabela_entrar_escopo(ctx->tabela);
    for (int i = 0; i < bloco->bloco.n_stmts; i++) {
        tc_checar_stmt(bloco->bloco.stmts[i], ctx);
    }
    tabela_sair_escopo(ctx->tabela);
}

static void tc_checar_stmt(No *stmt, ContextoSemantico *ctx) {
    TipoIluluC *tipo_decl;
    TipoIluluC *tipo_valor;
    TipoIluluC *tipo_ret;

    if (!stmt) {
        return;
    }

    switch (stmt->tipo) {
        case NO_VAR_DECL:
            tipo_decl = stmt->var.tipo ? tipo_de_no_ast(stmt->var.tipo) : NULL;
            tipo_valor = stmt->var.valor ? tc_checar_expr(stmt->var.valor, ctx) : NULL;
            if (!tipo_decl && tipo_valor) {
                tipo_decl = tipo_copiar(tipo_valor);
            }
            if (!tipo_decl) {
                tipo_decl = tipo_desconhecido();
            }
            if (tipo_valor && !tipo_atribuivel(tipo_decl, tipo_valor)) {
                tc_reportar(ctx, stmt, "Inicializador de variavel com tipo incompativel");
            }
            tabela_inserir(ctx->tabela,
                           stmt->var.nome,
                           SIMB_VAR,
                           tipo_copiar(tipo_decl),
                           stmt);
            stmt->tipo_inferido = tipo_copiar(tipo_decl);
            break;
        case NO_CONST_DECL:
            tipo_valor = stmt->constante.valor ? tc_checar_expr(stmt->constante.valor, ctx) : tipo_desconhecido();
            tabela_inserir(ctx->tabela,
                           stmt->constante.nome,
                           SIMB_CONST,
                           tipo_copiar(tipo_valor),
                           stmt);
            stmt->tipo_inferido = tipo_copiar(tipo_valor);
            break;
        case NO_BLOCO:
            tc_checar_bloco(stmt, ctx);
            break;
        case NO_IF:
            tipo_valor = tc_checar_expr(stmt->se.cond, ctx);
            if (!tipo_eh_verdadeiro(tipo_valor)) {
                tc_reportar(ctx, stmt->se.cond, "Condicao de if precisa ser booleana ou condicional");
            }
            tc_checar_stmt(stmt->se.entao, ctx);
            tc_checar_stmt(stmt->se.senao, ctx);
            break;
        case NO_WHILE:
            tipo_valor = tc_checar_expr(stmt->enquanto.cond, ctx);
            if (!tipo_eh_verdadeiro(tipo_valor)) {
                tc_reportar(ctx, stmt->enquanto.cond, "Condicao de while precisa ser booleana ou condicional");
            }
            tabela_entrar_loop(ctx->tabela);
            tc_checar_stmt(stmt->enquanto.corpo, ctx);
            tabela_sair_loop(ctx->tabela);
            break;
        case NO_LOOP_FOREVER:
            tabela_entrar_loop(ctx->tabela);
            tc_checar_stmt(stmt->loop_eterno.corpo, ctx);
            tabela_sair_loop(ctx->tabela);
            break;
        case NO_LOOP_FOR:
            tc_checar_expr(stmt->loop_for.inicio, ctx);
            tc_checar_expr(stmt->loop_for.fim, ctx);
            tabela_entrar_escopo(ctx->tabela);
            tabela_entrar_loop(ctx->tabela);
            tabela_inserir(ctx->tabela, stmt->loop_for.var, SIMB_VAR, tipo_criar(TIPO_I32), stmt);
            tc_checar_stmt(stmt->loop_for.corpo, ctx);
            tabela_sair_loop(ctx->tabela);
            tabela_sair_escopo(ctx->tabela);
            break;
        case NO_RETURN:
            tipo_ret = tabela_tipo_retorno(ctx->tabela);
            tipo_valor = stmt->retorno.valor ? tc_checar_expr(stmt->retorno.valor, ctx) : tipo_void();
            if (tipo_ret && !tipo_atribuivel(tipo_ret, tipo_valor)) {
                tc_reportar(ctx, stmt, "Tipo de retorno incompativel com a funcao");
            }
            break;
        case NO_MATCH:
            tc_checar_expr(stmt->match.expr, ctx);
            for (int i = 0; i < stmt->match.n_bracos; i++) {
                tc_checar_stmt(stmt->match.bracos[i]->braco.corpo, ctx);
            }
            break;
        case NO_UNSAFE:
            tc_checar_stmt(stmt->bloco_unsafe.bloco, ctx);
            break;
        case NO_EXPR_STMT:
            tc_checar_expr(stmt->expr_stmt.expr, ctx);
            break;
        case NO_BREAK:
            if (!tabela_dentro_loop(ctx->tabela)) {
                tc_reportar(ctx, stmt, "'break' fora de loop");
            }
            break;
        case NO_CONTINUE:
            if (!tabela_dentro_loop(ctx->tabela)) {
                tc_reportar(ctx, stmt, "'continue' fora de loop");
            }
            break;
        default:
            break;
    }
}

static void tc_checar_funcao(No *decl, ContextoSemantico *ctx) {
    TipoIluluC *tipo_ret = decl->funcao.tipo_ret ? tipo_de_no_ast(decl->funcao.tipo_ret) : tipo_void();

    tabela_entrar_funcao(ctx->tabela, tipo_ret);
    for (int i = 0; i < decl->funcao.n_params; i++) {
        No *param = decl->funcao.params[i];
        tabela_inserir(ctx->tabela,
                       param->param.nome,
                       SIMB_PARAM,
                       tipo_de_no_ast(param->param.tipo),
                       param);
    }

    tc_checar_stmt(decl->funcao.corpo, ctx);
    tabela_sair_funcao(ctx->tabela);
}

static void tc_checar_decl(No *decl, ContextoSemantico *ctx) {
    if (!decl) {
        return;
    }

    switch (decl->tipo) {
        case NO_FUNCAO_DECL:
            tc_checar_funcao(decl, ctx);
            break;
        case NO_NAMESPACE:
            for (int i = 0; i < decl->espaco.n_itens; i++) {
                tc_checar_decl(decl->espaco.itens[i], ctx);
            }
            break;
        case NO_CONST_DECL:
            if (decl->constante.valor) {
                tc_checar_expr(decl->constante.valor, ctx);
            }
            break;
        default:
            break;
    }
}

int type_checker_verificar(No *programa, ContextoSemantico *ctx) {
    if (!programa || !ctx) {
        return 0;
    }

    for (int i = 0; i < programa->programa.n_decls; i++) {
        tc_checar_decl(programa->programa.decls[i], ctx);
    }

    return !erros_tem_erros(ctx->erros);
}
