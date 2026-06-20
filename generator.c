#include "generator.h"

#include <string.h>

#include "types.h"

typedef struct {
    FILE *saida;
    int recuo;
} EstadoGerador;

static void emit_indent(EstadoGerador *g) {
    for (int i = 0; i < g->recuo; i++) {
        fputs("    ", g->saida);
    }
}

static void emit_string(FILE *saida, const char *texto) {
    fputc('"', saida);
    for (const char *p = texto; p && *p; p++) {
        switch (*p) {
            case '\\': fputs("\\\\", saida); break;
            case '"': fputs("\\\"", saida); break;
            case '\n': fputs("\\n", saida); break;
            case '\t': fputs("\\t", saida); break;
            case '\r': fputs("\\r", saida); break;
            default: fputc(*p, saida); break;
        }
    }
    fputc('"', saida);
}

static void emit_char(FILE *saida, char valor) {
    fputc('\'', saida);
    switch (valor) {
        case '\\': fputs("\\\\", saida); break;
        case '\'': fputs("\\'", saida); break;
        case '\n': fputs("\\n", saida); break;
        case '\t': fputs("\\t", saida); break;
        case '\r': fputs("\\r", saida); break;
        default: fputc(valor, saida); break;
    }
    fputc('\'', saida);
}

static void emit_expr(EstadoGerador *g, No *expr);
static void emit_stmt(EstadoGerador *g, No *stmt);
static void emit_decl(EstadoGerador *g, No *decl);

static int funcao_eh_main(No *decl) {
    return decl
        && decl->tipo == NO_FUNCAO_DECL
        && decl->funcao.nome
        && strcmp(decl->funcao.nome, "main") == 0;
}

static int bloco_termina_com_return(No *bloco) {
    if (!bloco || bloco->tipo != NO_BLOCO || bloco->bloco.n_stmts == 0) {
        return 0;
    }
    return bloco->bloco.stmts[bloco->bloco.n_stmts - 1]->tipo == NO_RETURN;
}

static void formatar_tipo_ast(No *no_tipo, const char *nome, char *buf, int buf_sz) {
    TipoIluluC *tipo = no_tipo ? tipo_de_no_ast(no_tipo) : tipo_void();
    tipo_formato_c(tipo, nome, buf, buf_sz);
    if (no_tipo) {
        tipo_liberar_recursivo(tipo);
    }
}

static void formatar_tipo_inferido(TipoIluluC *tipo, const char *nome, char *buf, int buf_sz) {
    tipo_formato_c(tipo ? tipo : tipo_desconhecido(), nome, buf, buf_sz);
}

static void emit_lista_args(EstadoGerador *g, No **args, int n_args) {
    for (int i = 0; i < n_args; i++) {
        if (i > 0) {
            fputs(", ", g->saida);
        }
        emit_expr(g, args[i]);
    }
}

static void emit_expr(EstadoGerador *g, No *expr) {
    if (!expr) {
        fputs("0", g->saida);
        return;
    }

    switch (expr->tipo) {
        case NO_BINARIO:
            fputc('(', g->saida);
            emit_expr(g, expr->binario.esq);
            fprintf(g->saida, " %s ", token_nome(expr->binario.op));
            emit_expr(g, expr->binario.dir);
            fputc(')', g->saida);
            break;
        case NO_UNARIO:
            fprintf(g->saida, "%s(", token_nome(expr->unario.op));
            emit_expr(g, expr->unario.operando);
            fputc(')', g->saida);
            break;
        case NO_CHAMADA:
            if (expr->chamada.funcao
                && expr->chamada.funcao->tipo == NO_IDENT
                && strcmp(expr->chamada.funcao->ident.nome, "print") == 0) {
                fputs("puts(", g->saida);
                if (expr->chamada.n_args > 0) {
                    emit_expr(g, expr->chamada.args[0]);
                } else {
                    emit_string(g->saida, "");
                }
                fputc(')', g->saida);
                break;
            }
            emit_expr(g, expr->chamada.funcao);
            fputc('(', g->saida);
            emit_lista_args(g, expr->chamada.args, expr->chamada.n_args);
            fputc(')', g->saida);
            break;
        case NO_INDEX:
            emit_expr(g, expr->index.array);
            fputc('[', g->saida);
            emit_expr(g, expr->index.indice);
            fputc(']', g->saida);
            break;
        case NO_CAMPO_ACESSO:
            emit_expr(g, expr->campo_acesso.objeto);
            fprintf(g->saida, ".%s", expr->campo_acesso.campo);
            break;
        case NO_CAST: {
            char tipo_buf[TIPO_BUF_MAX];
            formatar_tipo_ast(expr->cast.tipo, NULL, tipo_buf, sizeof(tipo_buf));
            fprintf(g->saida, "((%s)(", tipo_buf);
            emit_expr(g, expr->cast.expr);
            fputs("))", g->saida);
            break;
        }
        case NO_ADDRESS:
            fputs("&(", g->saida);
            emit_expr(g, expr->ref.expr);
            fputc(')', g->saida);
            break;
        case NO_DEREF:
            fputs("*(", g->saida);
            emit_expr(g, expr->ref.expr);
            fputc(')', g->saida);
            break;
        case NO_IDENT:
            fputs(expr->ident.nome, g->saida);
            break;
        case NO_LIT_INT:
            fprintf(g->saida, "%lld", expr->lit_int.valor);
            break;
        case NO_LIT_FLOAT:
            fprintf(g->saida, "%f", expr->lit_float.valor);
            break;
        case NO_LIT_STRING:
            emit_string(g->saida, expr->lit_str.valor);
            break;
        case NO_LIT_CHAR:
            emit_char(g->saida, expr->lit_char.valor);
            break;
        case NO_LIT_BOOL:
            fputs(expr->lit_bool.valor ? "true" : "false", g->saida);
            break;
        case NO_LIT_NULL:
            fputs("NULL", g->saida);
            break;
        default:
            fputs("0", g->saida);
            break;
    }
}

static void emit_bloco(EstadoGerador *g, No *bloco) {
    fputs("{\n", g->saida);
    g->recuo++;
    for (int i = 0; bloco && i < bloco->bloco.n_stmts; i++) {
        emit_stmt(g, bloco->bloco.stmts[i]);
    }
    g->recuo--;
    emit_indent(g);
    fputs("}\n", g->saida);
}

static void emit_stmt(EstadoGerador *g, No *stmt) {
    char tipo_buf[TIPO_BUF_MAX];

    if (!stmt) {
        return;
    }

    switch (stmt->tipo) {
        case NO_BLOCO:
            emit_indent(g);
            emit_bloco(g, stmt);
            break;
        case NO_VAR_DECL:
            emit_indent(g);
            if (stmt->var.tipo) {
                formatar_tipo_ast(stmt->var.tipo, stmt->var.nome, tipo_buf, sizeof(tipo_buf));
            } else {
                formatar_tipo_inferido(stmt->tipo_inferido, stmt->var.nome, tipo_buf, sizeof(tipo_buf));
            }
            fputs(tipo_buf, g->saida);
            if (stmt->var.valor) {
                fputs(" = ", g->saida);
                emit_expr(g, stmt->var.valor);
            }
            fputs(";\n", g->saida);
            break;
        case NO_CONST_DECL:
            emit_indent(g);
            formatar_tipo_inferido(stmt->tipo_inferido, stmt->constante.nome, tipo_buf, sizeof(tipo_buf));
            fprintf(g->saida, "const %s = ", tipo_buf);
            emit_expr(g, stmt->constante.valor);
            fputs(";\n", g->saida);
            break;
        case NO_IF:
            emit_indent(g);
            fputs("if (", g->saida);
            emit_expr(g, stmt->se.cond);
            fputs(") ", g->saida);
            emit_stmt(g, stmt->se.entao);
            if (stmt->se.senao) {
                emit_indent(g);
                fputs("else ", g->saida);
                emit_stmt(g, stmt->se.senao);
            }
            break;
        case NO_WHILE:
            emit_indent(g);
            fputs("while (", g->saida);
            emit_expr(g, stmt->enquanto.cond);
            fputs(") ", g->saida);
            emit_stmt(g, stmt->enquanto.corpo);
            break;
        case NO_LOOP_FOREVER:
            emit_indent(g);
            fputs("for (;;) ", g->saida);
            emit_stmt(g, stmt->loop_eterno.corpo);
            break;
        case NO_LOOP_FOR:
            emit_indent(g);
            fprintf(g->saida, "for (int32_t %s = ", stmt->loop_for.var);
            emit_expr(g, stmt->loop_for.inicio);
            fprintf(g->saida, "; %s <= ", stmt->loop_for.var);
            emit_expr(g, stmt->loop_for.fim);
            fprintf(g->saida, "; ++%s) ", stmt->loop_for.var);
            emit_stmt(g, stmt->loop_for.corpo);
            break;
        case NO_BREAK:
            emit_indent(g);
            fputs("break;\n", g->saida);
            break;
        case NO_CONTINUE:
            emit_indent(g);
            fputs("continue;\n", g->saida);
            break;
        case NO_RETURN:
            emit_indent(g);
            fputs("return", g->saida);
            if (stmt->retorno.valor) {
                fputc(' ', g->saida);
                emit_expr(g, stmt->retorno.valor);
            }
            fputs(";\n", g->saida);
            break;
        case NO_MATCH:
            for (int i = 0; i < stmt->match.n_bracos; i++) {
                emit_indent(g);
                if (i == 0) {
                    fputs("if (", g->saida);
                } else {
                    fputs("else if (", g->saida);
                }
                emit_expr(g, stmt->match.expr);
                fprintf(g->saida, " == %s) ", stmt->match.bracos[i]->braco.padrao);
                emit_stmt(g, stmt->match.bracos[i]->braco.corpo);
            }
            break;
        case NO_UNSAFE:
            emit_indent(g);
            emit_bloco(g, stmt->bloco_unsafe.bloco);
            break;
        case NO_EXPR_STMT:
            emit_indent(g);
            emit_expr(g, stmt->expr_stmt.expr);
            fputs(";\n", g->saida);
            break;
        default:
            emit_indent(g);
            fputs("/* statement nao suportado */\n", g->saida);
            break;
    }
}

static void emit_params(EstadoGerador *g, No **params, int n_params) {
    char tipo_buf[TIPO_BUF_MAX];

    if (n_params == 0) {
        fputs("void", g->saida);
        return;
    }

    for (int i = 0; i < n_params; i++) {
        if (i > 0) {
            fputs(", ", g->saida);
        }
        formatar_tipo_ast(params[i]->param.tipo, params[i]->param.nome, tipo_buf, sizeof(tipo_buf));
        fputs(tipo_buf, g->saida);
    }
}

static void emit_prototype(EstadoGerador *g, No *decl) {
    char tipo_buf[TIPO_BUF_MAX];

    if (!decl) {
        return;
    }

    switch (decl->tipo) {
        case NO_FUNCAO_DECL:
            if (funcao_eh_main(decl)) {
                fputs("int main(", g->saida);
            } else {
                formatar_tipo_ast(decl->funcao.tipo_ret, decl->funcao.nome, tipo_buf, sizeof(tipo_buf));
                fprintf(g->saida, "%s(", tipo_buf);
            }
            emit_params(g, decl->funcao.params, decl->funcao.n_params);
            fputs(");\n", g->saida);
            break;
        case NO_EXTERN_DECL:
            formatar_tipo_ast(decl->externo.tipo_ret, decl->externo.nome, tipo_buf, sizeof(tipo_buf));
            fprintf(g->saida, "extern %s(", tipo_buf);
            emit_params(g, decl->externo.params, decl->externo.n_params);
            fputs(");\n", g->saida);
            break;
        case NO_NAMESPACE:
            for (int i = 0; i < decl->espaco.n_itens; i++) {
                emit_prototype(g, decl->espaco.itens[i]);
            }
            break;
        default:
            break;
    }
}

static void emit_decl(EstadoGerador *g, No *decl) {
    char tipo_buf[TIPO_BUF_MAX];

    if (!decl) {
        return;
    }

    switch (decl->tipo) {
        case NO_STRUCT_DECL:
            fprintf(g->saida, "typedef struct %s {\n", decl->estrutura.nome);
            g->recuo++;
            for (int i = 0; i < decl->estrutura.n_campos; i++) {
                emit_indent(g);
                formatar_tipo_ast(decl->estrutura.campos[i]->campo.tipo,
                                  decl->estrutura.campos[i]->campo.nome,
                                  tipo_buf,
                                  sizeof(tipo_buf));
                fprintf(g->saida, "%s;\n", tipo_buf);
            }
            g->recuo--;
            fprintf(g->saida, "} %s;\n\n", decl->estrutura.nome);
            break;
        case NO_ENUM_DECL:
            fprintf(g->saida, "typedef enum %s {\n", decl->enumeracao.nome);
            g->recuo++;
            for (int i = 0; i < decl->enumeracao.n_variantes; i++) {
                emit_indent(g);
                fprintf(g->saida, "%s%s\n",
                        decl->enumeracao.variantes[i],
                        i + 1 < decl->enumeracao.n_variantes ? "," : "");
            }
            g->recuo--;
            fprintf(g->saida, "} %s;\n\n", decl->enumeracao.nome);
            break;
        case NO_ALIAS_TIPO:
            formatar_tipo_ast(decl->alias.tipo, decl->alias.nome, tipo_buf, sizeof(tipo_buf));
            fprintf(g->saida, "typedef %s;\n\n", tipo_buf);
            break;
        case NO_CONST_DECL:
            formatar_tipo_inferido(decl->tipo_inferido, decl->constante.nome, tipo_buf, sizeof(tipo_buf));
            fprintf(g->saida, "static const %s = ", tipo_buf);
            emit_expr(g, decl->constante.valor);
            fputs(";\n\n", g->saida);
            break;
        case NO_EXTERN_DECL:
            emit_prototype(g, decl);
            fputc('\n', g->saida);
            break;
        case NO_FUNCAO_DECL:
            if (funcao_eh_main(decl)) {
                fputs("int main(", g->saida);
            } else {
                formatar_tipo_ast(decl->funcao.tipo_ret, decl->funcao.nome, tipo_buf, sizeof(tipo_buf));
                fprintf(g->saida, "%s(", tipo_buf);
            }
            emit_params(g, decl->funcao.params, decl->funcao.n_params);
            fputs(") {\n", g->saida);
            g->recuo++;
            for (int i = 0; decl->funcao.corpo && i < decl->funcao.corpo->bloco.n_stmts; i++) {
                emit_stmt(g, decl->funcao.corpo->bloco.stmts[i]);
            }
            if (funcao_eh_main(decl) && !bloco_termina_com_return(decl->funcao.corpo)) {
                emit_indent(g);
                fputs("return 0;\n", g->saida);
            }
            g->recuo--;
            fputs("}\n", g->saida);
            fputc('\n', g->saida);
            break;
        case NO_NAMESPACE:
            fprintf(g->saida, "/* namespace %s */\n", decl->espaco.nome);
            for (int i = 0; i < decl->espaco.n_itens; i++) {
                emit_decl(g, decl->espaco.itens[i]);
            }
            break;
        case NO_USE_DECL:
            fprintf(g->saida, "/* use %s */\n", decl->uso.modulo);
            break;
        default:
            break;
    }
}

int gerador_emitir(No *programa,
                   ContextoSemantico *ctx,
                   FILE *saida,
                   const OpcoesGerador *opcoes) {
    EstadoGerador g;

    (void)ctx;

    if (!programa || !saida) {
        return 1;
    }

    g.saida = saida;
    g.recuo = 0;

    fprintf(saida, "/* Gerado por IluluC a partir de %s */\n",
            opcoes && opcoes->arquivo_fonte ? opcoes->arquivo_fonte : "<desconhecido>");
    fputs("#include <stdbool.h>\n", saida);
    fputs("#include <stdint.h>\n", saida);
    fputs("#include <stddef.h>\n", saida);
    fputs("#include <stdio.h>\n\n", saida);

    for (int i = 0; i < programa->programa.n_decls; i++) {
        emit_prototype(&g, programa->programa.decls[i]);
    }
    fputc('\n', saida);

    for (int i = 0; i < programa->programa.n_decls; i++) {
        emit_decl(&g, programa->programa.decls[i]);
    }

    return ferror(saida) ? 1 : 0;
}
