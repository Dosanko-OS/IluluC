/*
 * lexer.c - Implementação do analisador léxico do IluluC
 *
 * Converte código fonte em tokens um por um.
 * Suporta: identificadores, palavras-chave, literais (int, float, string, char),
 * operadores, delimitadores e comentários de linha e bloco.
 */

#include "lexer.h"
#include "token.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Declaração antecipada da função auxiliar de busca de palavra-chave */
TipoToken token_buscar_palavra_chave(const char *lexema);

/* ===== Funções auxiliares de inspeção de caracteres ===== */

/* Retorna o caractere atual sem avançar */
static char atual(const Lexer *lex) {
    return lex->fonte[lex->pos];
}

/* Retorna o caractere seguinte sem avançar */
static char seguinte(const Lexer *lex) {
    if (lex->fonte[lex->pos] == '\0') return '\0';
    return lex->fonte[lex->pos + 1];
}

/* Avança um caractere, atualizando linha e coluna */
static void avancar(Lexer *lex) {
    if (lex->fonte[lex->pos] == '\0') return;

    if (lex->fonte[lex->pos] == '\n') {
        lex->linha++;
        lex->coluna = 1;
    } else {
        lex->coluna++;
    }
    lex->pos++;
}

/* Captura a localização atual */
static Localizacao loc_atual(const Lexer *lex) {
    return loc_criar(lex->arquivo, lex->linha, lex->coluna);
}

/* ===== Pulo de espaços e comentários ===== */

/* Pula espaços em branco */
static void pular_espacos(Lexer *lex) {
    while (atual(lex) && isspace((unsigned char)atual(lex))) {
        avancar(lex);
    }
}

/* Pula comentário de linha: // até fim da linha */
static void pular_comentario_linha(Lexer *lex) {
    while (atual(lex) && atual(lex) != '\n') {
        avancar(lex);
    }
}

/* Pula comentario de bloco delimitado por slash-star e star-slash. */
static void pular_comentario_bloco(Lexer *lex, Localizacao loc_inicio) {
    avancar(lex); /* pula o '*' de abertura */
    while (atual(lex)) {
        if (atual(lex) == '*' && seguinte(lex) == '/') {
            avancar(lex); /* pula '*' */
            avancar(lex); /* pula '/' */
            return;
        }
        avancar(lex);
    }
    /* Chegou no EOF sem fechar o comentário */
    erros_adicionar(lex->erros, ERRO_LEXICO, loc_inicio,
        "Comentário de bloco não fechado (faltou '*/')");
}

/* Pula espaços e comentários, repetindo até não haver mais nada para pular */
static void pular_triviais(Lexer *lex) {
    for (;;) {
        pular_espacos(lex);

        if (atual(lex) == '/' && seguinte(lex) == '/') {
            avancar(lex); avancar(lex); /* pula '//' */
            pular_comentario_linha(lex);
            continue;
        }

        if (atual(lex) == '/' && seguinte(lex) == '*') {
            Localizacao loc = loc_atual(lex);
            avancar(lex); avancar(lex); /* pula abertura de comentario de bloco */
            pular_comentario_bloco(lex, loc);
            continue;
        }

        break;
    }
}

/* ===== Leitura de literais ===== */

/* Lê um número inteiro ou float */
static Token ler_numero(Lexer *lex) {
    Localizacao loc  = loc_atual(lex);
    int         ini  = lex->pos;
    int         eh_float = 0;

    /* Dígitos inteiros */
    while (atual(lex) && isdigit((unsigned char)atual(lex))) {
        avancar(lex);
    }

    /* Parte decimal */
    if (atual(lex) == '.' && isdigit((unsigned char)seguinte(lex))) {
        eh_float = 1;
        avancar(lex); /* pula '.' */
        while (atual(lex) && isdigit((unsigned char)atual(lex))) {
            avancar(lex);
        }
    }

    /* Extrai o lexema */
    int  tamanho = lex->pos - ini;
    char buf[64];
    if (tamanho >= 64) tamanho = 63;
    strncpy(buf, lex->fonte + ini, tamanho);
    buf[tamanho] = '\0';

    Token t = token_criar(eh_float ? TOKEN_LIT_FLOAT : TOKEN_LIT_INT, buf, loc);

    if (eh_float) {
        t.valor_float = atof(buf);
    } else {
        t.valor_int = (long long)atoll(buf);
    }

    return t;
}

/* Interpreta sequência de escape dentro de string ou char */
static char interpretar_escape(Lexer *lex) {
    avancar(lex); /* pula '\' */
    char c = atual(lex);
    avancar(lex);
    switch (c) {
        case 'n':  return '\n';
        case 't':  return '\t';
        case 'r':  return '\r';
        case '0':  return '\0';
        case '\\': return '\\';
        case '"':  return '"';
        case '\'': return '\'';
        default:
            return c; /* retorna o caractere literalmente */
    }
}

/* Lê uma literal string: "..." */
static Token ler_string(Lexer *lex) {
    Localizacao loc = loc_atual(lex);
    avancar(lex); /* pula '"' de abertura */

    /* Buffer para o conteúdo da string */
    int   cap    = 64;
    int   tam    = 0;
    char *buf    = malloc(cap);
    if (!buf) {
        return token_criar(TOKEN_EOF, "", loc);
    }

    while (atual(lex) && atual(lex) != '"') {
        char c;
        if (atual(lex) == '\\') {
            c = interpretar_escape(lex);
        } else if (atual(lex) == '\n') {
            erros_adicionar(lex->erros, ERRO_LEXICO, loc,
                "String não fechada antes do fim da linha");
            break;
        } else {
            c = atual(lex);
            avancar(lex);
        }

        /* Garante espaço no buffer */
        if (tam + 1 >= cap) {
            cap *= 2;
            char *novo = realloc(buf, cap);
            if (!novo) { free(buf); return token_criar(TOKEN_EOF, "", loc); }
            buf = novo;
        }
        buf[tam++] = c;
    }

    if (atual(lex) == '"') {
        avancar(lex); /* pula '"' de fechamento */
    } else {
        erros_adicionar(lex->erros, ERRO_LEXICO, loc, "String não fechada");
    }

    buf[tam] = '\0';
    Token t = token_criar(TOKEN_LIT_STRING, buf, loc);
    free(buf);
    return t;
}

/* Lê uma literal char: 'a' */
static Token ler_char(Lexer *lex) {
    Localizacao loc = loc_atual(lex);
    avancar(lex); /* pula '\'' de abertura */

    char c;
    if (atual(lex) == '\\') {
        c = interpretar_escape(lex);
    } else {
        c = atual(lex);
        avancar(lex);
    }

    if (atual(lex) == '\'') {
        avancar(lex); /* pula '\'' de fechamento */
    } else {
        erros_adicionar(lex->erros, ERRO_LEXICO, loc,
            "Literal char deve ter exatamente um caractere");
    }

    char buf[4];
    buf[0] = c; buf[1] = '\0';
    Token t = token_criar(TOKEN_LIT_CHAR, buf, loc);
    t.valor_int = (long long)c;
    return t;
}

/* Lê um identificador ou palavra-chave */
static Token ler_ident(Lexer *lex) {
    Localizacao loc = loc_atual(lex);
    int         ini = lex->pos;

    /* Identificador: letra ou '_', seguido de letras, dígitos ou '_' */
    while (atual(lex) && (isalnum((unsigned char)atual(lex)) || atual(lex) == '_')) {
        avancar(lex);
    }

    int   tam = lex->pos - ini;
    char  buf[128];
    if (tam >= 128) tam = 127;
    strncpy(buf, lex->fonte + ini, tam);
    buf[tam] = '\0';

    /* Verifica se é palavra-chave */
    TipoToken tipo = token_buscar_palavra_chave(buf);
    return token_criar(tipo, buf, loc);
}

/* ===== Leitura de operadores e delimitadores ===== */

static Token ler_operador(Lexer *lex) {
    Localizacao loc = loc_atual(lex);
    char        c   = atual(lex);
    char        nx  = seguinte(lex);

    /* Operadores de dois caracteres */
    if (c == '=' && nx == '=') { avancar(lex); avancar(lex); return token_criar(TOKEN_IGUAL_IGUAL, "==", loc); }
    if (c == '!' && nx == '=') { avancar(lex); avancar(lex); return token_criar(TOKEN_DIFERENTE,   "!=", loc); }
    if (c == '>' && nx == '=') { avancar(lex); avancar(lex); return token_criar(TOKEN_MAIOR_IGUAL, ">=", loc); }
    if (c == '<' && nx == '=') { avancar(lex); avancar(lex); return token_criar(TOKEN_MENOR_IGUAL, "<=", loc); }
    if (c == '&' && nx == '&') { avancar(lex); avancar(lex); return token_criar(TOKEN_E_E,         "&&", loc); }
    if (c == '|' && nx == '|') { avancar(lex); avancar(lex); return token_criar(TOKEN_OU_OU,       "||", loc); }
    if (c == '-' && nx == '>') { avancar(lex); avancar(lex); return token_criar(TOKEN_SETA,        "->", loc); }
    if (c == '=' && nx == '>') { avancar(lex); avancar(lex); return token_criar(TOKEN_FAT_SETA,    "=>", loc); }

    /* Operadores de um caractere */
    avancar(lex);
    switch (c) {
        case '+': return token_criar(TOKEN_MAIS,        "+", loc);
        case '-': return token_criar(TOKEN_MENOS,       "-", loc);
        case '*': return token_criar(TOKEN_STAR,        "*", loc);
        case '/': return token_criar(TOKEN_BARRA,       "/", loc);
        case '%': return token_criar(TOKEN_PORCENTO,    "%", loc);
        case '=': return token_criar(TOKEN_IGUAL,       "=", loc);
        case '>': return token_criar(TOKEN_MAIOR,       ">", loc);
        case '<': return token_criar(TOKEN_MENOR,       "<", loc);
        case '!': return token_criar(TOKEN_EXCLAMACAO,  "!", loc);
        case '{': return token_criar(TOKEN_ABRE_CHAVE,  "{", loc);
        case '}': return token_criar(TOKEN_FECHA_CHAVE, "}", loc);
        case '(': return token_criar(TOKEN_ABRE_PAREN,  "(", loc);
        case ')': return token_criar(TOKEN_FECHA_PAREN, ")", loc);
        case '[': return token_criar(TOKEN_ABRE_COL,    "[", loc);
        case ']': return token_criar(TOKEN_FECHA_COL,   "]", loc);
        case ',': return token_criar(TOKEN_VIRGULA,     ",", loc);
        case ':': return token_criar(TOKEN_DOIS_PONTOS, ":", loc);
        case '.': return token_criar(TOKEN_PONTO,       ".", loc);
        default: {
            char msg[32];
            snprintf(msg, sizeof(msg), "Caractere inesperado '%c'", c);
            erros_adicionar(lex->erros, ERRO_LEXICO, loc, "%s", msg);
            return token_criar(TOKEN_EOF, "", loc);
        }
    }
}

/* ===== Interface pública ===== */

/* Inicializa o lexer */
void lexer_init(Lexer *lex,
                const char *fonte,
                const char *arquivo,
                ContextoErros *erros) {
    lex->fonte   = fonte;
    lex->pos     = 0;
    lex->linha   = 1;
    lex->coluna  = 1;
    lex->arquivo = arquivo;
    lex->erros   = erros;
}

/* Retorna o próximo token e avança no texto */
Token lexer_proximo(Lexer *lex) {
    pular_triviais(lex);

    if (atual(lex) == '\0') {
        return token_criar(TOKEN_EOF, "", loc_atual(lex));
    }

    char c = atual(lex);

    if (isdigit((unsigned char)c)) return ler_numero(lex);
    if (c == '"')                  return ler_string(lex);
    if (c == '\'')                 return ler_char(lex);
    if (isalpha((unsigned char)c) || c == '_') return ler_ident(lex);

    return ler_operador(lex);
}

/* Salva o estado do lexer, obtém o próximo token e restaura o estado */
Token lexer_espiar(Lexer *lex) {
    int   pos_salva    = lex->pos;
    int   linha_salva  = lex->linha;
    int   coluna_salva = lex->coluna;

    Token t = lexer_proximo(lex);

    lex->pos    = pos_salva;
    lex->linha  = linha_salva;
    lex->coluna = coluna_salva;

    return t;
}
