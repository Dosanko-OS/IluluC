/*
 * parser.c - Implementação do analisador sintático do IluluC
 *
 * Descida recursiva pura. Cada construção da gramática tem sua função.
 * Expressões usam análise de precedência (Pratt parser).
 *
 * Gramática simplificada:
 *   programa    := declaracao*
 *   declaracao  := funcao | var_decl | const_decl | struct | enum
 *                | alias | namespace | extern | use
 *   stmt        := var_decl | const_decl | if | while | loop | match
 *                | return | break | continue | unsafe | expr_stmt
 *   expr        := Pratt com binding power
 */

#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Funções internas de navegação no token stream ===== */

/* Avança: descarta o token atual e puxa o próximo */
static void avancar(Parser *p) {
    token_destruir(&p->atual);
    p->atual   = p->proximo;
    p->proximo = lexer_proximo(p->lex);
}

/* Verifica se o token atual é do tipo esperado */
static int atual_eh(const Parser *p, TipoToken tipo) {
    return p->atual.tipo == tipo;
}

/* Consome o token atual se for do tipo esperado; caso contrário, adiciona erro */
static int esperar(Parser *p, TipoToken tipo) {
    if (p->atual.tipo == tipo) {
        avancar(p);
        return 1;
    }
    erros_adicionar(p->erros, ERRO_SINTATICO, p->atual.loc,
        "Esperado %s, encontrado %s",
        token_nome(tipo), token_nome(p->atual.tipo));
    return 0;
}

/* Consome e devolve uma cópia do lexema atual (avanpçando) */
static char *consumir_ident(Parser *p) {
    if (!atual_eh(p, TOKEN_IDENT)) {
        erros_adicionar(p->erros, ERRO_SINTATICO, p->atual.loc,
            "Esperado identificador, encontrado %s", token_nome(p->atual.tipo));
        return strdup("<erro>");
    }
    char *nome = strdup(p->atual.lexema);
    avancar(p);
    return nome;
}

/* ===== Declarações antecipadas ===== */
static No *parsear_expr(Parser *p, int min_bp);
static No *parsear_bloco(Parser *p);
static No *parsear_stmt(Parser *p);
static No *parsear_tipo(Parser *p);
static No *parsear_var_decl(Parser *p);

/* ===== Detecção de início de declaração de variável ===== */

/* Retorna 1 se o token atual pode iniciar uma declaração de variável/tipo */
static int eh_inicio_tipo(TipoToken tipo) {
    switch (tipo) {
        case TOKEN_U8:  case TOKEN_U16: case TOKEN_U32: case TOKEN_U64:
        case TOKEN_I8:  case TOKEN_I16: case TOKEN_I32: case TOKEN_I64:
        case TOKEN_F32: case TOKEN_F64:
        case TOKEN_BOOL: case TOKEN_CHAR: case TOKEN_STRING: case TOKEN_VOID:
        case TOKEN_POINTER:
            return 1;
        default:
            return 0;
    }
}

/* Dentro de bloco: TOKEN_IDENT seguido de TOKEN_IDENT = declaração de variável */
static int eh_var_decl(const Parser *p) {
    if (eh_inicio_tipo(p->atual.tipo)) return 1;
    if (p->atual.tipo == TOKEN_IDENT && p->proximo.tipo == TOKEN_IDENT) return 1;
    return 0;
}

/* ===== Parser de tipos ===== */

/* Parseia um nó de tipo: u32, pointer<u8>, string, NomeDeStruct, list<u32>, etc. */
static No *parsear_tipo(Parser *p) {
    Localizacao loc = p->atual.loc;

    /* pointer<T> */
    if (atual_eh(p, TOKEN_POINTER)) {
        avancar(p);
        esperar(p, TOKEN_MENOR);
        No *subtipo = parsear_tipo(p);
        esperar(p, TOKEN_MAIOR);

        No *no = no_criar(NO_TIPO, loc);
        if (!no) return NULL;
        no->tipo_no.categoria = TIPO_PONTEIRO;
        no->tipo_no.subtipo   = subtipo;
        return no;
    }

    /* Tipos primitivos */
    CategoriaType cat = TIPO_DESCONHECIDO;
    switch (p->atual.tipo) {
        case TOKEN_U8:     cat = TIPO_U8;     break;
        case TOKEN_U16:    cat = TIPO_U16;    break;
        case TOKEN_U32:    cat = TIPO_U32;    break;
        case TOKEN_U64:    cat = TIPO_U64;    break;
        case TOKEN_I8:     cat = TIPO_I8;     break;
        case TOKEN_I16:    cat = TIPO_I16;    break;
        case TOKEN_I32:    cat = TIPO_I32;    break;
        case TOKEN_I64:    cat = TIPO_I64;    break;
        case TOKEN_F32:    cat = TIPO_F32;    break;
        case TOKEN_F64:    cat = TIPO_F64;    break;
        case TOKEN_BOOL:   cat = TIPO_BOOL;   break;
        case TOKEN_CHAR:   cat = TIPO_CHAR;   break;
        case TOKEN_STRING: cat = TIPO_STRING; break;
        case TOKEN_VOID:   cat = TIPO_VOID;   break;
        default: break;
    }

    if (cat != TIPO_DESCONHECIDO) {
        avancar(p);
        No *no = no_criar(NO_TIPO, loc);
        if (!no) return NULL;
        no->tipo_no.categoria = cat;
        return no;
    }

    /* Tipo nomeado: NomeStruct ou genérico list<T> */
    if (atual_eh(p, TOKEN_IDENT)) {
        char *nome = strdup(p->atual.lexema);
        avancar(p);

        /* Genérico: Nome<T> */
        if (atual_eh(p, TOKEN_MENOR)) {
            avancar(p);
            No *subtipo = parsear_tipo(p);
            esperar(p, TOKEN_MAIOR);

            No *no = no_criar(NO_TIPO, loc);
            if (!no) { free(nome); return NULL; }
            no->tipo_no.categoria = TIPO_GENERICO;
            no->tipo_no.nome      = nome;
            no->tipo_no.subtipo   = subtipo;
            return no;
        }

        /* Tipo nomeado simples */
        No *no = no_criar(NO_TIPO, loc);
        if (!no) { free(nome); return NULL; }
        no->tipo_no.categoria = TIPO_STRUCT; /* será refinado pelo type checker */
        no->tipo_no.nome      = nome;
        return no;
    }

    erros_adicionar(p->erros, ERRO_SINTATICO, loc,
        "Tipo esperado, encontrado %s", token_nome(p->atual.tipo));
    return NULL;
}

/* ===== Expressões (Pratt parser) ===== */

/* Binding powers para cada operador binário (esquerda, direita) */
typedef struct { int esq; int dir; } BP;

static BP binding_power(TipoToken op) {
    switch (op) {
        case TOKEN_IGUAL:       return (BP){1, 2};   /* atribuição, direita-assoc */
        case TOKEN_OU_OU:       return (BP){3, 4};
        case TOKEN_E_E:         return (BP){5, 6};
        case TOKEN_IGUAL_IGUAL:
        case TOKEN_DIFERENTE:   return (BP){7, 8};
        case TOKEN_MENOR:
        case TOKEN_MAIOR:
        case TOKEN_MENOR_IGUAL:
        case TOKEN_MAIOR_IGUAL: return (BP){9, 10};
        case TOKEN_MAIS:
        case TOKEN_MENOS:       return (BP){11, 12};
        case TOKEN_STAR:
        case TOKEN_BARRA:
        case TOKEN_PORCENTO:    return (BP){13, 14};
        default:                return (BP){0, 0};
    }
}

/* Parseia lista de argumentos de chamada: '(' expr (',' expr)* ')' */
static No *parsear_args(Parser *p, No *funcao_no) {
    Localizacao loc = p->atual.loc;
    esperar(p, TOKEN_ABRE_PAREN);

    No *chamada = no_criar(NO_CHAMADA, loc);
    if (!chamada) return NULL;

    chamada->chamada.funcao = funcao_no;
    chamada->chamada.args   = NULL;
    chamada->chamada.n_args = 0;

    if (!atual_eh(p, TOKEN_FECHA_PAREN)) {
        int cap = 4;
        chamada->chamada.args = malloc(sizeof(No*) * cap);

        do {
            if (chamada->chamada.n_args >= cap) {
                cap *= 2;
                chamada->chamada.args = realloc(chamada->chamada.args, sizeof(No*) * cap);
            }
            No *arg = parsear_expr(p, 0);
            chamada->chamada.args[chamada->chamada.n_args++] = arg;
        } while (atual_eh(p, TOKEN_VIRGULA) && (avancar(p), 1));
    }

    esperar(p, TOKEN_FECHA_PAREN);
    return chamada;
}

/* Parseia o prefixo de uma expressão (Pratt: nud) */
static No *parsear_prefixo(Parser *p) {
    Localizacao loc = p->atual.loc;

    switch (p->atual.tipo) {

        /* --- Literais --- */
        case TOKEN_LIT_INT: {
            long long v = p->atual.valor_int;
            avancar(p);
            return no_lit_int(v, loc);
        }
        case TOKEN_LIT_FLOAT: {
            double v = p->atual.valor_float;
            avancar(p);
            return no_lit_float(v, loc);
        }
        case TOKEN_LIT_STRING: {
            char *v = strdup(p->atual.lexema);
            avancar(p);
            No *n = no_lit_string(v, loc);
            free(v);
            return n;
        }
        case TOKEN_LIT_CHAR: {
            char v = (char)p->atual.valor_int;
            avancar(p);
            return no_lit_char(v, loc);
        }
        case TOKEN_TRUE:  { avancar(p); return no_lit_bool(1, loc); }
        case TOKEN_FALSE: { avancar(p); return no_lit_bool(0, loc); }
        case TOKEN_NULL:  { avancar(p); return no_lit_null(loc); }

        /* --- Identificador ou chamada de função --- */
        case TOKEN_IDENT: {
            char *nome = strdup(p->atual.lexema);
            avancar(p);
            No *ident = no_ident(nome, loc);
            free(nome);

            /* Chamada de função: ident(...) */
            if (atual_eh(p, TOKEN_ABRE_PAREN)) {
                return parsear_args(p, ident);
            }
            return ident;
        }

        /* --- Parênteses agrupadores --- */
        case TOKEN_ABRE_PAREN: {
            avancar(p);
            No *expr = parsear_expr(p, 0);
            esperar(p, TOKEN_FECHA_PAREN);
            return expr;
        }

        /* --- Negação e NOT --- */
        case TOKEN_MENOS: {
            avancar(p);
            No *operando = parsear_expr(p, 15);
            No *n = no_criar(NO_UNARIO, loc);
            if (!n) return NULL;
            n->unario.op       = TOKEN_MENOS;
            n->unario.operando = operando;
            return n;
        }
        case TOKEN_EXCLAMACAO: {
            avancar(p);
            No *operando = parsear_expr(p, 15);
            No *n = no_criar(NO_UNARIO, loc);
            if (!n) return NULL;
            n->unario.op       = TOKEN_EXCLAMACAO;
            n->unario.operando = operando;
            return n;
        }

        /* --- address(expr) --- */
        case TOKEN_ADDRESS: {
            avancar(p);
            esperar(p, TOKEN_ABRE_PAREN);
            No *expr = parsear_expr(p, 0);
            esperar(p, TOKEN_FECHA_PAREN);
            No *n = no_criar(NO_ADDRESS, loc);
            if (!n) return NULL;
            n->ref.expr = expr;
            return n;
        }

        /* --- deref(expr) --- */
        case TOKEN_DEREF: {
            avancar(p);
            esperar(p, TOKEN_ABRE_PAREN);
            No *expr = parsear_expr(p, 0);
            esperar(p, TOKEN_FECHA_PAREN);
            No *n = no_criar(NO_DEREF, loc);
            if (!n) return NULL;
            n->ref.expr = expr;
            return n;
        }

        /* --- as<tipo>(expr) --- */
        case TOKEN_AS: {
            avancar(p);
            esperar(p, TOKEN_MENOR);
            No *tipo = parsear_tipo(p);
            esperar(p, TOKEN_MAIOR);
            esperar(p, TOKEN_ABRE_PAREN);
            No *expr = parsear_expr(p, 0);
            esperar(p, TOKEN_FECHA_PAREN);
            No *n = no_criar(NO_CAST, loc);
            if (!n) return NULL;
            n->cast.tipo = tipo;
            n->cast.expr = expr;
            return n;
        }

        default:
            erros_adicionar(p->erros, ERRO_SINTATICO, loc,
                "Expressão esperada, encontrado %s", token_nome(p->atual.tipo));
            avancar(p); /* recuperação de erro */
            return no_lit_null(loc);
    }
}

/* Parser de expressões com Pratt (precedência ascendente) */
static No *parsear_expr(Parser *p, int min_bp) {
    No *esq = parsear_prefixo(p);
    if (!esq) return NULL;

    for (;;) {
        TipoToken op = p->atual.tipo;

        /* Acesso a campo: objeto.campo */
        if (op == TOKEN_PONTO) {
            avancar(p);
            Localizacao loc = p->atual.loc;
            char *campo = consumir_ident(p);
            No *n = no_criar(NO_CAMPO_ACESSO, loc);
            if (!n) { free(campo); break; }
            n->campo_acesso.objeto = esq;
            n->campo_acesso.campo  = campo;
            esq = n;

            /* Pode ser chamada de método: obj.metodo(...) */
            if (atual_eh(p, TOKEN_ABRE_PAREN)) {
                esq = parsear_args(p, esq);
            }
            continue;
        }

        /* Indexação: array[indice] */
        if (op == TOKEN_ABRE_COL) {
            Localizacao loc = p->atual.loc;
            avancar(p);
            No *indice = parsear_expr(p, 0);
            esperar(p, TOKEN_FECHA_COL);
            No *n = no_criar(NO_INDEX, loc);
            if (!n) break;
            n->index.array  = esq;
            n->index.indice = indice;
            esq = n;
            continue;
        }

        /* Operadores binários */
        BP bp = binding_power(op);
        if (bp.esq == 0 || bp.esq < min_bp) break;

        Localizacao loc = p->atual.loc;
        avancar(p);

        No *dir = parsear_expr(p, bp.dir);
        No *n   = no_criar(NO_BINARIO, loc);
        if (!n) break;
        n->binario.op  = op;
        n->binario.esq = esq;
        n->binario.dir = dir;
        esq = n;
    }

    return esq;
}

/* ===== Statements ===== */

/* Parseia bloco: { stmt* } */
static No *parsear_bloco(Parser *p) {
    Localizacao loc = p->atual.loc;
    esperar(p, TOKEN_ABRE_CHAVE);

    No *bloco = no_criar(NO_BLOCO, loc);
    if (!bloco) return NULL;

    int cap = 8;
    bloco->bloco.stmts   = malloc(sizeof(No*) * cap);
    bloco->bloco.n_stmts = 0;

    while (!atual_eh(p, TOKEN_FECHA_CHAVE) && !atual_eh(p, TOKEN_EOF)) {
        if (bloco->bloco.n_stmts >= cap) {
            cap *= 2;
            bloco->bloco.stmts = realloc(bloco->bloco.stmts, sizeof(No*) * cap);
        }
        No *stmt = parsear_stmt(p);
        if (stmt) bloco->bloco.stmts[bloco->bloco.n_stmts++] = stmt;
    }

    esperar(p, TOKEN_FECHA_CHAVE);
    return bloco;
}

/* Parseia if/else */
static No *parsear_if(Parser *p) {
    Localizacao loc = p->atual.loc;
    esperar(p, TOKEN_IF);

    No *cond  = parsear_expr(p, 0);
    No *entao = parsear_bloco(p);
    No *senao = NULL;

    if (atual_eh(p, TOKEN_ELSE)) {
        avancar(p);
        if (atual_eh(p, TOKEN_IF)) {
            senao = parsear_if(p); /* else if encadeado */
        } else {
            senao = parsear_bloco(p);
        }
    }

    No *n = no_criar(NO_IF, loc);
    if (!n) return NULL;
    n->se.cond  = cond;
    n->se.entao = entao;
    n->se.senao = senao;
    return n;
}

/* Parseia while */
static No *parsear_while(Parser *p) {
    Localizacao loc = p->atual.loc;
    esperar(p, TOKEN_WHILE);

    No *cond  = parsear_expr(p, 0);
    p->profundidade++;
    No *corpo = parsear_bloco(p);
    p->profundidade--;

    No *n = no_criar(NO_WHILE, loc);
    if (!n) return NULL;
    n->enquanto.cond  = cond;
    n->enquanto.corpo = corpo;
    return n;
}

/* Parseia loop forever ou loop i from 0 to 100 */
static No *parsear_loop(Parser *p) {
    Localizacao loc = p->atual.loc;
    esperar(p, TOKEN_LOOP);

    /* loop forever { } */
    if (atual_eh(p, TOKEN_FOREVER)) {
        avancar(p);
        p->profundidade++;
        No *corpo = parsear_bloco(p);
        p->profundidade--;

        No *n = no_criar(NO_LOOP_FOREVER, loc);
        if (!n) return NULL;
        n->loop_eterno.corpo = corpo;
        return n;
    }

    /* loop i from expr to expr { } */
    char *var   = consumir_ident(p);
    esperar(p, TOKEN_FROM);
    No *inicio  = parsear_expr(p, 0);
    esperar(p, TOKEN_TO);
    No *fim     = parsear_expr(p, 0);
    p->profundidade++;
    No *corpo   = parsear_bloco(p);
    p->profundidade--;

    No *n = no_criar(NO_LOOP_FOR, loc);
    if (!n) { free(var); return NULL; }
    n->loop_for.var    = var;
    n->loop_for.inicio = inicio;
    n->loop_for.fim    = fim;
    n->loop_for.corpo  = corpo;
    return n;
}

/* Parseia match */
static No *parsear_match(Parser *p) {
    Localizacao loc = p->atual.loc;
    esperar(p, TOKEN_MATCH);

    No *expr = parsear_expr(p, 0);
    esperar(p, TOKEN_ABRE_CHAVE);

    No *match = no_criar(NO_MATCH, loc);
    if (!match) return NULL;
    match->match.expr    = expr;
    match->match.bracos  = NULL;
    match->match.n_bracos = 0;

    int cap = 4;
    match->match.bracos = malloc(sizeof(No*) * cap);

    while (!atual_eh(p, TOKEN_FECHA_CHAVE) && !atual_eh(p, TOKEN_EOF)) {
        Localizacao loc_braco = p->atual.loc;

        /* Padrão: identificador */
        char *padrao = consumir_ident(p);
        esperar(p, TOKEN_FAT_SETA);

        /* Corpo: bloco ou expressão simples */
        No *corpo;
        if (atual_eh(p, TOKEN_ABRE_CHAVE)) {
            corpo = parsear_bloco(p);
        } else {
            No *e = parsear_expr(p, 0);
            /* Empacota em NO_EXPR_STMT para uniformidade */
            corpo = no_criar(NO_EXPR_STMT, loc_braco);
            if (corpo) corpo->expr_stmt.expr = e;
        }

        No *braco = no_criar(NO_MATCH_BRACO, loc_braco);
        if (!braco) { free(padrao); break; }
        braco->braco.padrao = padrao;
        braco->braco.corpo  = corpo;

        if (match->match.n_bracos >= cap) {
            cap *= 2;
            match->match.bracos = realloc(match->match.bracos, sizeof(No*) * cap);
        }
        match->match.bracos[match->match.n_bracos++] = braco;
    }

    esperar(p, TOKEN_FECHA_CHAVE);
    return match;
}

/* Parseia return */
static No *parsear_return(Parser *p) {
    Localizacao loc = p->atual.loc;
    esperar(p, TOKEN_RETURN);

    No *valor = NULL;
    /* return com valor se o próximo token não for '}' ou EOF */
    if (!atual_eh(p, TOKEN_FECHA_CHAVE) && !atual_eh(p, TOKEN_EOF)) {
        valor = parsear_expr(p, 0);
    }

    No *n = no_criar(NO_RETURN, loc);
    if (!n) return NULL;
    n->retorno.valor = valor;
    return n;
}

/* Parseia declaração de variável: tipo nome ('[' INT ']')? ('=' expr)? */
static No *parsear_var_decl(Parser *p) {
    Localizacao loc = p->atual.loc;

    No *tipo = parsear_tipo(p);
    if (!tipo) return NULL;

    char *nome = consumir_ident(p);

    /* Array: tipo nome[tamanho] */
    if (atual_eh(p, TOKEN_ABRE_COL)) {
        avancar(p);
        int tam = 0;
        if (atual_eh(p, TOKEN_LIT_INT)) {
            tam = (int)p->atual.valor_int;
            avancar(p);
        }
        esperar(p, TOKEN_FECHA_COL);

        /* Encapsula o tipo em um NO_TIPO array */
        No *tipo_arr = no_criar(NO_TIPO, loc);
        if (tipo_arr) {
            tipo_arr->tipo_no.categoria = TIPO_ARRAY;
            tipo_arr->tipo_no.subtipo   = tipo;
            tipo_arr->tipo_no.tamanho   = tam;
        }
        tipo = tipo_arr;
    }

    No *valor = NULL;
    if (atual_eh(p, TOKEN_IGUAL)) {
        avancar(p);
        valor = parsear_expr(p, 0);
    }

    No *n = no_criar(NO_VAR_DECL, loc);
    if (!n) { free(nome); return NULL; }
    n->var.nome  = nome;
    n->var.tipo  = tipo;
    n->var.valor = valor;
    return n;
}

/* Parseia um statement qualquer */
static No *parsear_stmt(Parser *p) {
    Localizacao loc = p->atual.loc;

    switch (p->atual.tipo) {

        case TOKEN_IF:       return parsear_if(p);
        case TOKEN_WHILE:    return parsear_while(p);
        case TOKEN_LOOP:     return parsear_loop(p);
        case TOKEN_RETURN:   return parsear_return(p);
        case TOKEN_MATCH:    return parsear_match(p);

        case TOKEN_BREAK: {
            if (p->profundidade == 0) {
                erros_adicionar(p->erros, ERRO_SEMANTICO, loc,
                    "'break' fora de loop");
            }
            avancar(p);
            return no_criar(NO_BREAK, loc);
        }

        case TOKEN_CONTINUE: {
            if (p->profundidade == 0) {
                erros_adicionar(p->erros, ERRO_SEMANTICO, loc,
                    "'continue' fora de loop");
            }
            avancar(p);
            return no_criar(NO_CONTINUE, loc);
        }

        case TOKEN_UNSAFE: {
            avancar(p);
            No *bloco = parsear_bloco(p);
            No *n = no_criar(NO_UNSAFE, loc);
            if (!n) return NULL;
            n->bloco_unsafe.bloco = bloco;
            return n;
        }

        case TOKEN_CONST: {
            avancar(p);
            char *nome = consumir_ident(p);
            esperar(p, TOKEN_IGUAL);
            No *valor = parsear_expr(p, 0);
            No *n = no_criar(NO_CONST_DECL, loc);
            if (!n) { free(nome); return NULL; }
            n->constante.nome  = nome;
            n->constante.valor = valor;
            return n;
        }

        default:
            /* Declaração de variável ou expressão */
            if (eh_var_decl(p)) {
                return parsear_var_decl(p);
            }
            /* Statement de expressão */
            {
                No *expr = parsear_expr(p, 0);
                No *n = no_criar(NO_EXPR_STMT, loc);
                if (!n) return NULL;
                n->expr_stmt.expr = expr;
                return n;
            }
    }
}

/* ===== Declarações de topo de arquivo ===== */

/* Parseia lista de parâmetros: '(' (nome ':' tipo (',' nome ':' tipo)*)? ')' */
static void parsear_lista_params(Parser *p, No ***out_params, int *out_n) {
    esperar(p, TOKEN_ABRE_PAREN);

    int  cap    = 4;
    No **params = malloc(sizeof(No*) * cap);
    int  n      = 0;

    while (!atual_eh(p, TOKEN_FECHA_PAREN) && !atual_eh(p, TOKEN_EOF)) {
        if (n > 0) esperar(p, TOKEN_VIRGULA);

        Localizacao loc  = p->atual.loc;
        char       *nome = consumir_ident(p);
        esperar(p, TOKEN_DOIS_PONTOS);
        No         *tipo = parsear_tipo(p);

        No *param = no_criar(NO_PARAM, loc);
        if (param) {
            param->param.nome = nome;
            param->param.tipo = tipo;
        } else {
            free(nome);
        }

        if (n >= cap) {
            cap *= 2;
            params = realloc(params, sizeof(No*) * cap);
        }
        params[n++] = param;
    }

    esperar(p, TOKEN_FECHA_PAREN);
    *out_params = params;
    *out_n      = n;
}

/* Parseia declaração de função */
static No *parsear_funcao(Parser *p) {
    Localizacao loc = p->atual.loc;
    esperar(p, TOKEN_FUNCTION);

    char *nome = consumir_ident(p);

    No **params; int n_params;
    parsear_lista_params(p, &params, &n_params);

    /* Tipo de retorno opcional: -> tipo */
    No *tipo_ret = NULL;
    if (atual_eh(p, TOKEN_SETA)) {
        avancar(p);
        tipo_ret = parsear_tipo(p);
    }

    int salvo = p->em_funcao;
    p->em_funcao = 1;
    No *corpo = parsear_bloco(p);
    p->em_funcao = salvo;

    No *n = no_criar(NO_FUNCAO_DECL, loc);
    if (!n) { free(nome); return NULL; }
    n->funcao.nome     = nome;
    n->funcao.params   = params;
    n->funcao.n_params = n_params;
    n->funcao.tipo_ret = tipo_ret;
    n->funcao.corpo    = corpo;
    return n;
}

/* Parseia declaração de struct */
static No *parsear_struct(Parser *p) {
    Localizacao loc = p->atual.loc;
    esperar(p, TOKEN_STRUCT);

    char *nome = consumir_ident(p);
    esperar(p, TOKEN_ABRE_CHAVE);

    int  cap    = 4;
    No **campos = malloc(sizeof(No*) * cap);
    int  n      = 0;

    while (!atual_eh(p, TOKEN_FECHA_CHAVE) && !atual_eh(p, TOKEN_EOF)) {
        Localizacao loc_campo = p->atual.loc;
        char *nome_campo = consumir_ident(p);
        esperar(p, TOKEN_DOIS_PONTOS);
        No *tipo = parsear_tipo(p);

        No *campo = no_criar(NO_STRUCT_CAMPO, loc_campo);
        if (campo) {
            campo->campo.nome = nome_campo;
            campo->campo.tipo = tipo;
        } else {
            free(nome_campo);
        }

        if (n >= cap) { cap *= 2; campos = realloc(campos, sizeof(No*) * cap); }
        campos[n++] = campo;
    }

    esperar(p, TOKEN_FECHA_CHAVE);

    No *no = no_criar(NO_STRUCT_DECL, loc);
    if (!no) { free(nome); return NULL; }
    no->estrutura.nome    = nome;
    no->estrutura.campos  = campos;
    no->estrutura.n_campos = n;
    return no;
}

/* Parseia declaração de enum */
static No *parsear_enum(Parser *p) {
    Localizacao loc = p->atual.loc;
    esperar(p, TOKEN_ENUM);

    char *nome = consumir_ident(p);
    esperar(p, TOKEN_ABRE_CHAVE);

    int    cap       = 4;
    char **variantes = malloc(sizeof(char*) * cap);
    int    n         = 0;

    while (!atual_eh(p, TOKEN_FECHA_CHAVE) && !atual_eh(p, TOKEN_EOF)) {
        if (n >= cap) { cap *= 2; variantes = realloc(variantes, sizeof(char*) * cap); }
        variantes[n++] = consumir_ident(p);
    }

    esperar(p, TOKEN_FECHA_CHAVE);

    No *no = no_criar(NO_ENUM_DECL, loc);
    if (!no) { free(nome); return NULL; }
    no->enumeracao.nome       = nome;
    no->enumeracao.variantes  = variantes;
    no->enumeracao.n_variantes = n;
    return no;
}

/* Parseia extern func(...) -> tipo */
static No *parsear_extern(Parser *p) {
    Localizacao loc = p->atual.loc;
    esperar(p, TOKEN_EXTERN);

    char *nome = consumir_ident(p);

    No **params; int n_params;
    parsear_lista_params(p, &params, &n_params);

    No *tipo_ret = NULL;
    if (atual_eh(p, TOKEN_SETA)) {
        avancar(p);
        tipo_ret = parsear_tipo(p);
    }

    No *no = no_criar(NO_EXTERN_DECL, loc);
    if (!no) { free(nome); return NULL; }
    no->externo.nome     = nome;
    no->externo.params   = params;
    no->externo.n_params = n_params;
    no->externo.tipo_ret = tipo_ret;
    return no;
}

/* Parseia use modulo */
static No *parsear_use(Parser *p) {
    Localizacao loc = p->atual.loc;
    esperar(p, TOKEN_USE);
    char *modulo = consumir_ident(p);
    No *no = no_criar(NO_USE_DECL, loc);
    if (!no) { free(modulo); return NULL; }
    no->uso.modulo = modulo;
    return no;
}

/* Parseia namespace ns { declaracao* } */
static No *parsear_namespace(Parser *p);

/* Parseia type nome = tipo */
static No *parsear_alias(Parser *p) {
    Localizacao loc = p->atual.loc;
    esperar(p, TOKEN_TYPE);
    char *nome = consumir_ident(p);
    esperar(p, TOKEN_IGUAL);
    No *tipo = parsear_tipo(p);
    No *no = no_criar(NO_ALIAS_TIPO, loc);
    if (!no) { free(nome); return NULL; }
    no->alias.nome = nome;
    no->alias.tipo = tipo;
    return no;
}

/* Parseia uma declaração de nível superior */
static No *parsear_declaracao(Parser *p) {
    switch (p->atual.tipo) {
        case TOKEN_FUNCTION:  return parsear_funcao(p);
        case TOKEN_STRUCT:    return parsear_struct(p);
        case TOKEN_ENUM:      return parsear_enum(p);
        case TOKEN_EXTERN:    return parsear_extern(p);
        case TOKEN_USE:       return parsear_use(p);
        case TOKEN_NAMESPACE: return parsear_namespace(p);
        case TOKEN_TYPE:      return parsear_alias(p);
        case TOKEN_CONST: {
            Localizacao loc = p->atual.loc;
            avancar(p);
            char *nome = consumir_ident(p);
            esperar(p, TOKEN_IGUAL);
            No *valor = parsear_expr(p, 0);
            No *n = no_criar(NO_CONST_DECL, loc);
            if (!n) { free(nome); return NULL; }
            n->constante.nome  = nome;
            n->constante.valor = valor;
            return n;
        }
        default:
            erros_adicionar(p->erros, ERRO_SINTATICO, p->atual.loc,
                "Declaração esperada, encontrado %s", token_nome(p->atual.tipo));
            avancar(p); /* recuperação de erro */
            return NULL;
    }
}

/* Parseia namespace ns { ... } */
static No *parsear_namespace(Parser *p) {
    Localizacao loc = p->atual.loc;
    esperar(p, TOKEN_NAMESPACE);
    char *nome = consumir_ident(p);
    esperar(p, TOKEN_ABRE_CHAVE);

    int  cap   = 4;
    No **itens = malloc(sizeof(No*) * cap);
    int  n     = 0;

    while (!atual_eh(p, TOKEN_FECHA_CHAVE) && !atual_eh(p, TOKEN_EOF)) {
        if (n >= cap) { cap *= 2; itens = realloc(itens, sizeof(No*) * cap); }
        No *decl = parsear_declaracao(p);
        if (decl) itens[n++] = decl;
    }

    esperar(p, TOKEN_FECHA_CHAVE);

    No *no = no_criar(NO_NAMESPACE, loc);
    if (!no) { free(nome); return NULL; }
    no->espaco.nome   = nome;
    no->espaco.itens  = itens;
    no->espaco.n_itens = n;
    return no;
}

/* ===== Interface pública ===== */

/* Inicializa o parser */
void parser_init(Parser *p, Lexer *lex, ContextoErros *erros) {
    p->lex         = lex;
    p->erros       = erros;
    p->profundidade = 0;
    p->em_funcao   = 0;

    /* Carrega os dois primeiros tokens */
    p->atual   = lexer_proximo(lex);
    p->proximo = lexer_proximo(lex);
}

/* Parseia o programa completo e retorna o nó raiz */
No *parser_parsear(Parser *p) {
    Localizacao loc = p->atual.loc;

    No *prog = no_criar(NO_PROGRAMA, loc);
    if (!prog) return NULL;

    int  cap   = 16;
    prog->programa.decls   = malloc(sizeof(No*) * cap);
    prog->programa.n_decls = 0;

    while (!atual_eh(p, TOKEN_EOF)) {
        if (prog->programa.n_decls >= cap) {
            cap *= 2;
            prog->programa.decls = realloc(prog->programa.decls, sizeof(No*) * cap);
        }
        No *decl = parsear_declaracao(p);
        if (decl) prog->programa.decls[prog->programa.n_decls++] = decl;

        /* Recuperação de erro: se não avançamos, pulamos o token problemático */
        if (atual_eh(p, TOKEN_EOF)) break;
    }

    return prog;
}

/* Libera os tokens internos do parser */
void parser_destruir(Parser *p) {
    token_destruir(&p->atual);
    token_destruir(&p->proximo);
}
