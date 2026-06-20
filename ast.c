#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ast_strdup(const char *texto) {
    size_t len = strlen(texto) + 1;
    char *copia = malloc(len);
    if (!copia) {
        return NULL;
    }
    memcpy(copia, texto, len);
    return copia;
}

static void imprimir_indentacao(int recuo) {
    for (int i = 0; i < recuo; i++) {
        fputs("  ", stdout);
    }
}

No *no_criar(TipoNo tipo, Localizacao loc) {
    No *no = calloc(1, sizeof(No));
    if (!no) {
        return NULL;
    }
    no->tipo = tipo;
    no->loc = loc;
    no->tipo_inferido = NULL;
    return no;
}

static void no_destruir_lista(No **nos, int quantidade) {
    if (!nos) {
        return;
    }
    for (int i = 0; i < quantidade; i++) {
        no_destruir(nos[i]);
    }
    free(nos);
}

static void strings_destruir(char **strings, int quantidade) {
    if (!strings) {
        return;
    }
    for (int i = 0; i < quantidade; i++) {
        free(strings[i]);
    }
    free(strings);
}

void no_destruir(No *no) {
    if (!no) {
        return;
    }

    switch (no->tipo) {
        case NO_PROGRAMA:
            no_destruir_lista(no->programa.decls, no->programa.n_decls);
            break;
        case NO_FUNCAO_DECL:
            free(no->funcao.nome);
            no_destruir_lista(no->funcao.params, no->funcao.n_params);
            no_destruir(no->funcao.tipo_ret);
            no_destruir(no->funcao.corpo);
            break;
        case NO_PARAM:
            free(no->param.nome);
            no_destruir(no->param.tipo);
            break;
        case NO_VAR_DECL:
            free(no->var.nome);
            no_destruir(no->var.tipo);
            no_destruir(no->var.valor);
            break;
        case NO_CONST_DECL:
            free(no->constante.nome);
            no_destruir(no->constante.valor);
            break;
        case NO_STRUCT_DECL:
            free(no->estrutura.nome);
            no_destruir_lista(no->estrutura.campos, no->estrutura.n_campos);
            break;
        case NO_STRUCT_CAMPO:
            free(no->campo.nome);
            no_destruir(no->campo.tipo);
            break;
        case NO_ENUM_DECL:
            free(no->enumeracao.nome);
            strings_destruir(no->enumeracao.variantes, no->enumeracao.n_variantes);
            break;
        case NO_ALIAS_TIPO:
            free(no->alias.nome);
            no_destruir(no->alias.tipo);
            break;
        case NO_NAMESPACE:
            free(no->espaco.nome);
            no_destruir_lista(no->espaco.itens, no->espaco.n_itens);
            break;
        case NO_EXTERN_DECL:
            free(no->externo.nome);
            no_destruir_lista(no->externo.params, no->externo.n_params);
            no_destruir(no->externo.tipo_ret);
            break;
        case NO_USE_DECL:
            free(no->uso.modulo);
            break;
        case NO_BLOCO:
            no_destruir_lista(no->bloco.stmts, no->bloco.n_stmts);
            break;
        case NO_IF:
            no_destruir(no->se.cond);
            no_destruir(no->se.entao);
            no_destruir(no->se.senao);
            break;
        case NO_WHILE:
            no_destruir(no->enquanto.cond);
            no_destruir(no->enquanto.corpo);
            break;
        case NO_LOOP_FOREVER:
            no_destruir(no->loop_eterno.corpo);
            break;
        case NO_LOOP_FOR:
            free(no->loop_for.var);
            no_destruir(no->loop_for.inicio);
            no_destruir(no->loop_for.fim);
            no_destruir(no->loop_for.corpo);
            break;
        case NO_BREAK:
        case NO_CONTINUE:
        case NO_LIT_NULL:
            break;
        case NO_RETURN:
            no_destruir(no->retorno.valor);
            break;
        case NO_MATCH:
            no_destruir(no->match.expr);
            no_destruir_lista(no->match.bracos, no->match.n_bracos);
            break;
        case NO_MATCH_BRACO:
            free(no->braco.padrao);
            no_destruir(no->braco.corpo);
            break;
        case NO_UNSAFE:
            no_destruir(no->bloco_unsafe.bloco);
            break;
        case NO_EXPR_STMT:
            no_destruir(no->expr_stmt.expr);
            break;
        case NO_BINARIO:
            no_destruir(no->binario.esq);
            no_destruir(no->binario.dir);
            break;
        case NO_UNARIO:
            no_destruir(no->unario.operando);
            break;
        case NO_CHAMADA:
            no_destruir(no->chamada.funcao);
            no_destruir_lista(no->chamada.args, no->chamada.n_args);
            break;
        case NO_INDEX:
            no_destruir(no->index.array);
            no_destruir(no->index.indice);
            break;
        case NO_CAMPO_ACESSO:
            no_destruir(no->campo_acesso.objeto);
            free(no->campo_acesso.campo);
            break;
        case NO_CAST:
            no_destruir(no->cast.tipo);
            no_destruir(no->cast.expr);
            break;
        case NO_ADDRESS:
        case NO_DEREF:
            no_destruir(no->ref.expr);
            break;
        case NO_IDENT:
            free(no->ident.nome);
            break;
        case NO_LIT_INT:
        case NO_LIT_FLOAT:
        case NO_LIT_CHAR:
        case NO_LIT_BOOL:
            break;
        case NO_LIT_STRING:
            free(no->lit_str.valor);
            break;
        case NO_TIPO:
            free(no->tipo_no.nome);
            no_destruir(no->tipo_no.subtipo);
            break;
    }

    free(no);
}

static const char *no_nome(TipoNo tipo) {
    switch (tipo) {
        case NO_PROGRAMA: return "NO_PROGRAMA";
        case NO_FUNCAO_DECL: return "NO_FUNCAO_DECL";
        case NO_PARAM: return "NO_PARAM";
        case NO_VAR_DECL: return "NO_VAR_DECL";
        case NO_CONST_DECL: return "NO_CONST_DECL";
        case NO_STRUCT_DECL: return "NO_STRUCT_DECL";
        case NO_STRUCT_CAMPO: return "NO_STRUCT_CAMPO";
        case NO_ENUM_DECL: return "NO_ENUM_DECL";
        case NO_ALIAS_TIPO: return "NO_ALIAS_TIPO";
        case NO_NAMESPACE: return "NO_NAMESPACE";
        case NO_EXTERN_DECL: return "NO_EXTERN_DECL";
        case NO_USE_DECL: return "NO_USE_DECL";
        case NO_BLOCO: return "NO_BLOCO";
        case NO_IF: return "NO_IF";
        case NO_WHILE: return "NO_WHILE";
        case NO_LOOP_FOREVER: return "NO_LOOP_FOREVER";
        case NO_LOOP_FOR: return "NO_LOOP_FOR";
        case NO_BREAK: return "NO_BREAK";
        case NO_CONTINUE: return "NO_CONTINUE";
        case NO_RETURN: return "NO_RETURN";
        case NO_MATCH: return "NO_MATCH";
        case NO_MATCH_BRACO: return "NO_MATCH_BRACO";
        case NO_UNSAFE: return "NO_UNSAFE";
        case NO_EXPR_STMT: return "NO_EXPR_STMT";
        case NO_BINARIO: return "NO_BINARIO";
        case NO_UNARIO: return "NO_UNARIO";
        case NO_CHAMADA: return "NO_CHAMADA";
        case NO_INDEX: return "NO_INDEX";
        case NO_CAMPO_ACESSO: return "NO_CAMPO_ACESSO";
        case NO_CAST: return "NO_CAST";
        case NO_ADDRESS: return "NO_ADDRESS";
        case NO_DEREF: return "NO_DEREF";
        case NO_IDENT: return "NO_IDENT";
        case NO_LIT_INT: return "NO_LIT_INT";
        case NO_LIT_FLOAT: return "NO_LIT_FLOAT";
        case NO_LIT_STRING: return "NO_LIT_STRING";
        case NO_LIT_CHAR: return "NO_LIT_CHAR";
        case NO_LIT_BOOL: return "NO_LIT_BOOL";
        case NO_LIT_NULL: return "NO_LIT_NULL";
        case NO_TIPO: return "NO_TIPO";
        default: return "NO_DESCONHECIDO";
    }
}

static const char *categoria_tipo_nome(CategoriaType categoria) {
    switch (categoria) {
        case TIPO_U8: return "u8";
        case TIPO_U16: return "u16";
        case TIPO_U32: return "u32";
        case TIPO_U64: return "u64";
        case TIPO_I8: return "i8";
        case TIPO_I16: return "i16";
        case TIPO_I32: return "i32";
        case TIPO_I64: return "i64";
        case TIPO_F32: return "f32";
        case TIPO_F64: return "f64";
        case TIPO_BOOL: return "bool";
        case TIPO_CHAR: return "char";
        case TIPO_STRING: return "string";
        case TIPO_VOID: return "void";
        case TIPO_PONTEIRO: return "pointer";
        case TIPO_ARRAY: return "array";
        case TIPO_GENERICO: return "generico";
        case TIPO_STRUCT: return "struct";
        case TIPO_ENUM: return "enum";
        case TIPO_ALIAS: return "alias";
        case TIPO_DESCONHECIDO: return "desconhecido";
        default: return "<tipo>";
    }
}

static void no_imprimir_rotulo(const char *rotulo, const No *no, int recuo) {
    if (!no) {
        imprimir_indentacao(recuo);
        printf("%s: <null>\n", rotulo);
        return;
    }

    imprimir_indentacao(recuo);
    printf("%s:\n", rotulo);
    no_imprimir(no, recuo + 1);
}

static void no_imprimir_lista_rotulada(const char *rotulo, No *const *nos, int quantidade, int recuo) {
    imprimir_indentacao(recuo);
    printf("%s (%d)\n", rotulo, quantidade);
    for (int i = 0; i < quantidade; i++) {
        no_imprimir(nos[i], recuo + 1);
    }
}

void no_imprimir(const No *no, int recuo) {
    if (!no) {
        imprimir_indentacao(recuo);
        puts("<null>");
        return;
    }

    imprimir_indentacao(recuo);
    puts(no_nome(no->tipo));

    switch (no->tipo) {
        case NO_PROGRAMA:
            no_imprimir_lista_rotulada("decls", no->programa.decls, no->programa.n_decls, recuo + 1);
            break;
        case NO_FUNCAO_DECL:
            imprimir_indentacao(recuo + 1);
            printf("nome: %s\n", no->funcao.nome);
            no_imprimir_lista_rotulada("params", no->funcao.params, no->funcao.n_params, recuo + 1);
            no_imprimir_rotulo("tipo_ret", no->funcao.tipo_ret, recuo + 1);
            no_imprimir_rotulo("corpo", no->funcao.corpo, recuo + 1);
            break;
        case NO_PARAM:
            imprimir_indentacao(recuo + 1);
            printf("nome: %s\n", no->param.nome);
            no_imprimir_rotulo("tipo", no->param.tipo, recuo + 1);
            break;
        case NO_VAR_DECL:
            imprimir_indentacao(recuo + 1);
            printf("nome: %s\n", no->var.nome);
            no_imprimir_rotulo("tipo", no->var.tipo, recuo + 1);
            no_imprimir_rotulo("valor", no->var.valor, recuo + 1);
            break;
        case NO_CONST_DECL:
            imprimir_indentacao(recuo + 1);
            printf("nome: %s\n", no->constante.nome);
            no_imprimir_rotulo("valor", no->constante.valor, recuo + 1);
            break;
        case NO_STRUCT_DECL:
            imprimir_indentacao(recuo + 1);
            printf("nome: %s\n", no->estrutura.nome);
            no_imprimir_lista_rotulada("campos", no->estrutura.campos, no->estrutura.n_campos, recuo + 1);
            break;
        case NO_STRUCT_CAMPO:
            imprimir_indentacao(recuo + 1);
            printf("nome: %s\n", no->campo.nome);
            no_imprimir_rotulo("tipo", no->campo.tipo, recuo + 1);
            break;
        case NO_ENUM_DECL:
            imprimir_indentacao(recuo + 1);
            printf("nome: %s\n", no->enumeracao.nome);
            for (int i = 0; i < no->enumeracao.n_variantes; i++) {
                imprimir_indentacao(recuo + 1);
                printf("variante: %s\n", no->enumeracao.variantes[i]);
            }
            break;
        case NO_ALIAS_TIPO:
            imprimir_indentacao(recuo + 1);
            printf("nome: %s\n", no->alias.nome);
            no_imprimir_rotulo("tipo", no->alias.tipo, recuo + 1);
            break;
        case NO_NAMESPACE:
            imprimir_indentacao(recuo + 1);
            printf("nome: %s\n", no->espaco.nome);
            no_imprimir_lista_rotulada("itens", no->espaco.itens, no->espaco.n_itens, recuo + 1);
            break;
        case NO_EXTERN_DECL:
            imprimir_indentacao(recuo + 1);
            printf("nome: %s\n", no->externo.nome);
            no_imprimir_lista_rotulada("params", no->externo.params, no->externo.n_params, recuo + 1);
            no_imprimir_rotulo("tipo_ret", no->externo.tipo_ret, recuo + 1);
            break;
        case NO_USE_DECL:
            imprimir_indentacao(recuo + 1);
            printf("modulo: %s\n", no->uso.modulo);
            break;
        case NO_BLOCO:
            no_imprimir_lista_rotulada("stmts", no->bloco.stmts, no->bloco.n_stmts, recuo + 1);
            break;
        case NO_IF:
            no_imprimir_rotulo("cond", no->se.cond, recuo + 1);
            no_imprimir_rotulo("entao", no->se.entao, recuo + 1);
            no_imprimir_rotulo("senao", no->se.senao, recuo + 1);
            break;
        case NO_WHILE:
            no_imprimir_rotulo("cond", no->enquanto.cond, recuo + 1);
            no_imprimir_rotulo("corpo", no->enquanto.corpo, recuo + 1);
            break;
        case NO_LOOP_FOREVER:
            no_imprimir_rotulo("corpo", no->loop_eterno.corpo, recuo + 1);
            break;
        case NO_LOOP_FOR:
            imprimir_indentacao(recuo + 1);
            printf("var: %s\n", no->loop_for.var);
            no_imprimir_rotulo("inicio", no->loop_for.inicio, recuo + 1);
            no_imprimir_rotulo("fim", no->loop_for.fim, recuo + 1);
            no_imprimir_rotulo("corpo", no->loop_for.corpo, recuo + 1);
            break;
        case NO_RETURN:
            no_imprimir_rotulo("valor", no->retorno.valor, recuo + 1);
            break;
        case NO_MATCH:
            no_imprimir_rotulo("expr", no->match.expr, recuo + 1);
            no_imprimir_lista_rotulada("bracos", no->match.bracos, no->match.n_bracos, recuo + 1);
            break;
        case NO_MATCH_BRACO:
            imprimir_indentacao(recuo + 1);
            printf("padrao: %s\n", no->braco.padrao);
            no_imprimir_rotulo("corpo", no->braco.corpo, recuo + 1);
            break;
        case NO_UNSAFE:
            no_imprimir_rotulo("bloco", no->bloco_unsafe.bloco, recuo + 1);
            break;
        case NO_EXPR_STMT:
            no_imprimir_rotulo("expr", no->expr_stmt.expr, recuo + 1);
            break;
        case NO_BINARIO:
            imprimir_indentacao(recuo + 1);
            printf("op: %s\n", token_nome(no->binario.op));
            no_imprimir_rotulo("esq", no->binario.esq, recuo + 1);
            no_imprimir_rotulo("dir", no->binario.dir, recuo + 1);
            break;
        case NO_UNARIO:
            imprimir_indentacao(recuo + 1);
            printf("op: %s\n", token_nome(no->unario.op));
            no_imprimir_rotulo("operando", no->unario.operando, recuo + 1);
            break;
        case NO_CHAMADA:
            no_imprimir_rotulo("funcao", no->chamada.funcao, recuo + 1);
            no_imprimir_lista_rotulada("args", no->chamada.args, no->chamada.n_args, recuo + 1);
            break;
        case NO_INDEX:
            no_imprimir_rotulo("array", no->index.array, recuo + 1);
            no_imprimir_rotulo("indice", no->index.indice, recuo + 1);
            break;
        case NO_CAMPO_ACESSO:
            imprimir_indentacao(recuo + 1);
            printf("campo: %s\n", no->campo_acesso.campo);
            no_imprimir_rotulo("objeto", no->campo_acesso.objeto, recuo + 1);
            break;
        case NO_CAST:
            no_imprimir_rotulo("tipo", no->cast.tipo, recuo + 1);
            no_imprimir_rotulo("expr", no->cast.expr, recuo + 1);
            break;
        case NO_ADDRESS:
        case NO_DEREF:
            no_imprimir_rotulo("expr", no->ref.expr, recuo + 1);
            break;
        case NO_IDENT:
            imprimir_indentacao(recuo + 1);
            printf("nome: %s\n", no->ident.nome);
            break;
        case NO_LIT_INT:
            imprimir_indentacao(recuo + 1);
            printf("valor: %lld\n", no->lit_int.valor);
            break;
        case NO_LIT_FLOAT:
            imprimir_indentacao(recuo + 1);
            printf("valor: %f\n", no->lit_float.valor);
            break;
        case NO_LIT_STRING:
            imprimir_indentacao(recuo + 1);
            printf("valor: \"%s\"\n", no->lit_str.valor);
            break;
        case NO_LIT_CHAR:
            imprimir_indentacao(recuo + 1);
            printf("valor: '%c'\n", no->lit_char.valor);
            break;
        case NO_LIT_BOOL:
            imprimir_indentacao(recuo + 1);
            printf("valor: %s\n", no->lit_bool.valor ? "true" : "false");
            break;
        case NO_LIT_NULL:
            break;
        case NO_TIPO:
            imprimir_indentacao(recuo + 1);
            printf("categoria: %s\n", categoria_tipo_nome(no->tipo_no.categoria));
            if (no->tipo_no.nome) {
                imprimir_indentacao(recuo + 1);
                printf("nome: %s\n", no->tipo_no.nome);
            }
            no_imprimir_rotulo("subtipo", no->tipo_no.subtipo, recuo + 1);
            if (no->tipo_no.tamanho > 0) {
                imprimir_indentacao(recuo + 1);
                printf("tamanho: %d\n", no->tipo_no.tamanho);
            }
            break;
        case NO_BREAK:
        case NO_CONTINUE:
            break;
    }
}

No *no_lit_int(long long valor, Localizacao loc) {
    No *no = no_criar(NO_LIT_INT, loc);
    if (no) {
        no->lit_int.valor = valor;
    }
    return no;
}

No *no_lit_float(double valor, Localizacao loc) {
    No *no = no_criar(NO_LIT_FLOAT, loc);
    if (no) {
        no->lit_float.valor = valor;
    }
    return no;
}

No *no_lit_string(const char *valor, Localizacao loc) {
    No *no = no_criar(NO_LIT_STRING, loc);
    if (no) {
        no->lit_str.valor = ast_strdup(valor ? valor : "");
    }
    return no;
}

No *no_lit_char(char valor, Localizacao loc) {
    No *no = no_criar(NO_LIT_CHAR, loc);
    if (no) {
        no->lit_char.valor = valor;
    }
    return no;
}

No *no_lit_bool(int valor, Localizacao loc) {
    No *no = no_criar(NO_LIT_BOOL, loc);
    if (no) {
        no->lit_bool.valor = valor != 0;
    }
    return no;
}

No *no_lit_null(Localizacao loc) {
    return no_criar(NO_LIT_NULL, loc);
}

No *no_ident(const char *nome, Localizacao loc) {
    No *no = no_criar(NO_IDENT, loc);
    if (no) {
        no->ident.nome = ast_strdup(nome ? nome : "");
    }
    return no;
}

TipoIluluC *tipo_criar(CategoriaType cat) {
    TipoIluluC *tipo = calloc(1, sizeof(TipoIluluC));
    if (!tipo) {
        return NULL;
    }
    tipo->categoria = cat;
    return tipo;
}

TipoIluluC *tipo_ponteiro(TipoIluluC *subtipo) {
    TipoIluluC *tipo = tipo_criar(TIPO_PONTEIRO);
    if (tipo) {
        tipo->subtipo = subtipo;
    }
    return tipo;
}

TipoIluluC *tipo_array(TipoIluluC *subtipo, int tamanho) {
    TipoIluluC *tipo = tipo_criar(TIPO_ARRAY);
    if (tipo) {
        tipo->subtipo = subtipo;
        tipo->tamanho = tamanho;
    }
    return tipo;
}

void tipo_destruir(TipoIluluC *t) {
    if (!t) {
        return;
    }
    tipo_destruir(t->subtipo);
    free(t->nome);
    free(t);
}

int tipo_igual(const TipoIluluC *a, const TipoIluluC *b) {
    if (a == b) {
        return 1;
    }
    if (!a || !b || a->categoria != b->categoria || a->tamanho != b->tamanho) {
        return 0;
    }
    if ((a->nome || b->nome) && (!a->nome || !b->nome || strcmp(a->nome, b->nome) != 0)) {
        return 0;
    }
    return tipo_igual(a->subtipo, b->subtipo);
}

int tipo_eh_numerico(const TipoIluluC *t) {
    if (!t) {
        return 0;
    }
    switch (t->categoria) {
        case TIPO_U8:
        case TIPO_U16:
        case TIPO_U32:
        case TIPO_U64:
        case TIPO_I8:
        case TIPO_I16:
        case TIPO_I32:
        case TIPO_I64:
        case TIPO_F32:
        case TIPO_F64:
            return 1;
        default:
            return 0;
    }
}

int tipo_eh_inteiro(const TipoIluluC *t) {
    if (!t) {
        return 0;
    }
    switch (t->categoria) {
        case TIPO_U8:
        case TIPO_U16:
        case TIPO_U32:
        case TIPO_U64:
        case TIPO_I8:
        case TIPO_I16:
        case TIPO_I32:
        case TIPO_I64:
        case TIPO_CHAR:
            return 1;
        default:
            return 0;
    }
}

const char *tipo_nome_c(const TipoIluluC *t) {
    if (!t) {
        return "void";
    }

    switch (t->categoria) {
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
        default:
            return t->nome ? t->nome : "void";
    }
}

const char *tipo_nome_iluluc(const TipoIluluC *t) {
    if (!t) {
        return "void";
    }

    switch (t->categoria) {
        case TIPO_U8: return "u8";
        case TIPO_U16: return "u16";
        case TIPO_U32: return "u32";
        case TIPO_U64: return "u64";
        case TIPO_I8: return "i8";
        case TIPO_I16: return "i16";
        case TIPO_I32: return "i32";
        case TIPO_I64: return "i64";
        case TIPO_F32: return "f32";
        case TIPO_F64: return "f64";
        case TIPO_BOOL: return "bool";
        case TIPO_CHAR: return "char";
        case TIPO_STRING: return "string";
        case TIPO_VOID: return "void";
        case TIPO_PONTEIRO: return "pointer";
        case TIPO_ARRAY: return "array";
        case TIPO_GENERICO: return "generico";
        case TIPO_STRUCT: return t->nome ? t->nome : "struct";
        case TIPO_ENUM: return t->nome ? t->nome : "enum";
        case TIPO_ALIAS: return t->nome ? t->nome : "alias";
        case TIPO_DESCONHECIDO: return "<desconhecido>";
        default: return "<tipo>";
    }
}
