#include "token.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *texto;
    TipoToken tipo;
} PalavraChave;

static char *token_strdup(const char *texto) {
    size_t len = strlen(texto) + 1;
    char *copia = malloc(len);
    if (!copia) {
        return NULL;
    }
    memcpy(copia, texto, len);
    return copia;
}

TipoToken token_buscar_palavra_chave(const char *lexema) {
    static const PalavraChave palavras[] = {
        {"if", TOKEN_IF},
        {"else", TOKEN_ELSE},
        {"while", TOKEN_WHILE},
        {"loop", TOKEN_LOOP},
        {"forever", TOKEN_FOREVER},
        {"from", TOKEN_FROM},
        {"to", TOKEN_TO},
        {"break", TOKEN_BREAK},
        {"continue", TOKEN_CONTINUE},
        {"return", TOKEN_RETURN},
        {"match", TOKEN_MATCH},
        {"function", TOKEN_FUNCTION},
        {"struct", TOKEN_STRUCT},
        {"enum", TOKEN_ENUM},
        {"const", TOKEN_CONST},
        {"use", TOKEN_USE},
        {"namespace", TOKEN_NAMESPACE},
        {"type", TOKEN_TYPE},
        {"extern", TOKEN_EXTERN},
        {"unsafe", TOKEN_UNSAFE},
        {"pointer", TOKEN_POINTER},
        {"address", TOKEN_ADDRESS},
        {"deref", TOKEN_DEREF},
        {"as", TOKEN_AS},
        {"true", TOKEN_TRUE},
        {"false", TOKEN_FALSE},
        {"null", TOKEN_NULL},
        {"u8", TOKEN_U8},
        {"u16", TOKEN_U16},
        {"u32", TOKEN_U32},
        {"u64", TOKEN_U64},
        {"i8", TOKEN_I8},
        {"i16", TOKEN_I16},
        {"i32", TOKEN_I32},
        {"i64", TOKEN_I64},
        {"f32", TOKEN_F32},
        {"f64", TOKEN_F64},
        {"bool", TOKEN_BOOL},
        {"char", TOKEN_CHAR},
        {"string", TOKEN_STRING},
        {"void", TOKEN_VOID},
    };

    for (size_t i = 0; i < sizeof(palavras) / sizeof(palavras[0]); i++) {
        if (strcmp(lexema, palavras[i].texto) == 0) {
            return palavras[i].tipo;
        }
    }

    return TOKEN_IDENT;
}

const char *token_nome(TipoToken tipo) {
    switch (tipo) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_LIT_INT: return "literal inteiro";
        case TOKEN_LIT_FLOAT: return "literal float";
        case TOKEN_LIT_STRING: return "literal string";
        case TOKEN_LIT_CHAR: return "literal char";
        case TOKEN_IDENT: return "identificador";
        case TOKEN_IF: return "if";
        case TOKEN_ELSE: return "else";
        case TOKEN_WHILE: return "while";
        case TOKEN_LOOP: return "loop";
        case TOKEN_FOREVER: return "forever";
        case TOKEN_FROM: return "from";
        case TOKEN_TO: return "to";
        case TOKEN_BREAK: return "break";
        case TOKEN_CONTINUE: return "continue";
        case TOKEN_RETURN: return "return";
        case TOKEN_MATCH: return "match";
        case TOKEN_FUNCTION: return "function";
        case TOKEN_STRUCT: return "struct";
        case TOKEN_ENUM: return "enum";
        case TOKEN_CONST: return "const";
        case TOKEN_USE: return "use";
        case TOKEN_NAMESPACE: return "namespace";
        case TOKEN_TYPE: return "type";
        case TOKEN_EXTERN: return "extern";
        case TOKEN_UNSAFE: return "unsafe";
        case TOKEN_POINTER: return "pointer";
        case TOKEN_ADDRESS: return "address";
        case TOKEN_DEREF: return "deref";
        case TOKEN_AS: return "as";
        case TOKEN_TRUE: return "true";
        case TOKEN_FALSE: return "false";
        case TOKEN_NULL: return "null";
        case TOKEN_U8: return "u8";
        case TOKEN_U16: return "u16";
        case TOKEN_U32: return "u32";
        case TOKEN_U64: return "u64";
        case TOKEN_I8: return "i8";
        case TOKEN_I16: return "i16";
        case TOKEN_I32: return "i32";
        case TOKEN_I64: return "i64";
        case TOKEN_F32: return "f32";
        case TOKEN_F64: return "f64";
        case TOKEN_BOOL: return "bool";
        case TOKEN_CHAR: return "char";
        case TOKEN_STRING: return "string";
        case TOKEN_VOID: return "void";
        case TOKEN_MAIS: return "+";
        case TOKEN_MENOS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_BARRA: return "/";
        case TOKEN_PORCENTO: return "%";
        case TOKEN_IGUAL_IGUAL: return "==";
        case TOKEN_DIFERENTE: return "!=";
        case TOKEN_MAIOR: return ">";
        case TOKEN_MENOR: return "<";
        case TOKEN_MAIOR_IGUAL: return ">=";
        case TOKEN_MENOR_IGUAL: return "<=";
        case TOKEN_E_E: return "&&";
        case TOKEN_OU_OU: return "||";
        case TOKEN_EXCLAMACAO: return "!";
        case TOKEN_IGUAL: return "=";
        case TOKEN_ABRE_CHAVE: return "{";
        case TOKEN_FECHA_CHAVE: return "}";
        case TOKEN_ABRE_PAREN: return "(";
        case TOKEN_FECHA_PAREN: return ")";
        case TOKEN_ABRE_COL: return "[";
        case TOKEN_FECHA_COL: return "]";
        case TOKEN_VIRGULA: return ",";
        case TOKEN_DOIS_PONTOS: return ":";
        case TOKEN_SETA: return "->";
        case TOKEN_FAT_SETA: return "=>";
        case TOKEN_PONTO: return ".";
        case TOKEN__TOTAL: return "<total>";
        default: return "<token desconhecido>";
    }
}

Token token_criar(TipoToken tipo, const char *lexema, Localizacao loc) {
    Token t;
    t.tipo = tipo;
    t.lexema = token_strdup(lexema ? lexema : "");
    t.loc = loc;
    t.valor_int = 0;
    return t;
}

void token_destruir(Token *t) {
    if (!t) {
        return;
    }
    free(t->lexema);
    t->lexema = NULL;
    t->tipo = TOKEN_EOF;
    t->valor_int = 0;
}

int token_eh_tipo(TipoToken tipo) {
    switch (tipo) {
        case TOKEN_U8:
        case TOKEN_U16:
        case TOKEN_U32:
        case TOKEN_U64:
        case TOKEN_I8:
        case TOKEN_I16:
        case TOKEN_I32:
        case TOKEN_I64:
        case TOKEN_F32:
        case TOKEN_F64:
        case TOKEN_BOOL:
        case TOKEN_CHAR:
        case TOKEN_STRING:
        case TOKEN_VOID:
        case TOKEN_POINTER:
            return 1;
        default:
            return 0;
    }
}

int token_eh_op_binario(TipoToken tipo) {
    switch (tipo) {
        case TOKEN_IGUAL:
        case TOKEN_OU_OU:
        case TOKEN_E_E:
        case TOKEN_IGUAL_IGUAL:
        case TOKEN_DIFERENTE:
        case TOKEN_MENOR:
        case TOKEN_MAIOR:
        case TOKEN_MENOR_IGUAL:
        case TOKEN_MAIOR_IGUAL:
        case TOKEN_MAIS:
        case TOKEN_MENOS:
        case TOKEN_STAR:
        case TOKEN_BARRA:
        case TOKEN_PORCENTO:
            return 1;
        default:
            return 0;
    }
}
