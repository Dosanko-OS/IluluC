#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "error.h"
#include "generator.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "type_checker.h"
#include "types.h"

#ifndef ILULU_VERSION
#define ILULU_VERSION "1.0"
#endif

#ifndef ILULU_PKGREL
#define ILULU_PKGREL "1"
#endif

typedef enum {
    ACAO_AJUDA,
    ACAO_VERSAO,
    ACAO_COMPILAR,
    ACAO_EXECUTAR,
    ACAO_GERAR_C,
    ACAO_AST,
    ACAO_TOKENS,
    ACAO_FMT,
    ACAO_INIT
} AcaoCLI;

static void imprimir_uso(FILE *saida, const char *argv0) {
    fprintf(saida,
            "Uso:\n"
            "  %s help\n"
            "  %s --help\n"
            "  %s version\n"
            "  %s --version\n"
            "  %s comp <arquivo.ilc>\n"
            "  %s run <arquivo.ilc>\n"
            "  %s c <arquivo.ilc>\n"
            "  %s ast <arquivo.ilc>\n"
            "  %s tokens <arquivo.ilc>\n"
            "  %s fmt <arquivo.ilc>\n"
            "  %s init\n",
            argv0, argv0, argv0, argv0, argv0, argv0,
            argv0, argv0, argv0, argv0, argv0);
}

static void imprimir_versao(void) {
    printf("ilulu %s-%s\n", ILULU_VERSION, ILULU_PKGREL);
}

static char *duplicar_texto(const char *texto) {
    size_t len = strlen(texto) + 1;
    char *copia = malloc(len);
    if (!copia) {
        return NULL;
    }
    memcpy(copia, texto, len);
    return copia;
}

static char *ler_arquivo(const char *caminho) {
    FILE *fp;
    long tamanho;
    size_t lidos;
    char *conteudo;

    fp = fopen(caminho, "rb");
    if (!fp) {
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    tamanho = ftell(fp);
    if (tamanho < 0) {
        fclose(fp);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    conteudo = malloc((size_t)tamanho + 1);
    if (!conteudo) {
        fclose(fp);
        return NULL;
    }

    lidos = fread(conteudo, 1, (size_t)tamanho, fp);
    fclose(fp);
    if (lidos != (size_t)tamanho) {
        free(conteudo);
        return NULL;
    }

    conteudo[tamanho] = '\0';
    return conteudo;
}

static int escrever_arquivo(const char *caminho, const char *conteudo) {
    FILE *fp = fopen(caminho, "wb");
    size_t tamanho;

    if (!fp) {
        return 1;
    }

    tamanho = strlen(conteudo);
    if (fwrite(conteudo, 1, tamanho, fp) != tamanho) {
        fclose(fp);
        return 1;
    }

    return fclose(fp) != 0;
}

static char *substituir_extensao(const char *caminho, const char *nova_ext) {
    const char *ponto = strrchr(caminho, '.');
    size_t base_len = ponto ? (size_t)(ponto - caminho) : strlen(caminho);
    size_t ext_len = strlen(nova_ext);
    char *saida = malloc(base_len + ext_len + 1);
    if (!saida) {
        return NULL;
    }

    memcpy(saida, caminho, base_len);
    memcpy(saida + base_len, nova_ext, ext_len + 1);
    return saida;
}

static int nome_termina_com(const char *texto, const char *sufixo) {
    size_t len_texto = strlen(texto);
    size_t len_sufixo = strlen(sufixo);

    if (len_texto < len_sufixo) {
        return 0;
    }

    return strcmp(texto + len_texto - len_sufixo, sufixo) == 0;
}

static char *nome_executavel(const char *caminho) {
    return substituir_extensao(caminho, "");
}

static char *caminho_execucao(const char *arquivo_saida) {
    if (strchr(arquivo_saida, '/')) {
        return duplicar_texto(arquivo_saida);
    }

    size_t len = strlen(arquivo_saida);
    char *caminho = malloc(len + 3);
    if (!caminho) {
        return NULL;
    }

    memcpy(caminho, "./", 2);
    memcpy(caminho + 2, arquivo_saida, len + 1);
    return caminho;
}

static int executar_com_argv(char *const argv_exec[]) {
    pid_t pid;
    int status = 0;

    pid = fork();
    if (pid < 0) {
        return 1;
    }

    if (pid == 0) {
        execvp(argv_exec[0], argv_exec);
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0) {
        return 1;
    }

    if (!WIFEXITED(status)) {
        return 1;
    }

    return WEXITSTATUS(status);
}

static int compilar_c_para_executavel(const char *arquivo_c, const char *arquivo_saida) {
    const char *compiladores[3];
    char *argv_exec[10];
    int i;

    compiladores[0] = getenv("ILULU_CC");
    compiladores[1] = "gcc";
    compiladores[2] = "clang";

    for (i = 0; i < 3; i++) {
        if (!compiladores[i] || compiladores[i][0] == '\0') {
            continue;
        }

        argv_exec[0] = (char *)compiladores[i];
        argv_exec[1] = "-std=c17";
        argv_exec[2] = "-Wall";
        argv_exec[3] = "-Wextra";
        argv_exec[4] = "-Wpedantic";
        argv_exec[5] = "-g";
        argv_exec[6] = "-o";
        argv_exec[7] = (char *)arquivo_saida;
        argv_exec[8] = (char *)arquivo_c;
        argv_exec[9] = NULL;

        {
            pid_t pid = fork();
            int status = 0;

            if (pid < 0) {
                return 1;
            }

            if (pid == 0) {
                execvp(argv_exec[0], argv_exec);
                _exit(127);
            }

            if (waitpid(pid, &status, 0) < 0) {
                return 1;
            }

            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                return 0;
            }

            if (!(WIFEXITED(status) && WEXITSTATUS(status) == 127)) {
                return 1;
            }
        }
    }

    fprintf(stderr, "Falha ao encontrar um compilador C (gcc/clang)\n");
    return 1;
}

static int carregar_programa(const char *arquivo_fonte,
                             char **out_fonte,
                             ContextoErros **out_erros,
                             No **out_programa) {
    char *fonte;
    ContextoErros *erros;
    Lexer lexer;
    Parser parser;
    No *programa;

    fonte = ler_arquivo(arquivo_fonte);
    if (!fonte) {
        fprintf(stderr, "Falha ao ler '%s': %s\n", arquivo_fonte, strerror(errno));
        return 1;
    }

    erros = erros_criar();
    if (!erros) {
        fprintf(stderr, "Falha ao criar contexto de erros\n");
        free(fonte);
        return 1;
    }

    lexer_init(&lexer, fonte, arquivo_fonte, erros);
    parser_init(&parser, &lexer, erros);
    programa = parser_parsear(&parser);
    parser_destruir(&parser);

    if (erros_tem_erros(erros)) {
        erros_imprimir(erros);
        no_destruir(programa);
        erros_destruir(erros);
        free(fonte);
        return 1;
    }

    *out_fonte = fonte;
    *out_erros = erros;
    *out_programa = programa;
    return 0;
}

static int analisar_programa(No *programa,
                             ContextoErros *erros,
                             ContextoSemantico **out_sem) {
    ContextoSemantico *sem = semantica_analisar(programa, erros);
    if (!sem || erros_tem_erros(erros)) {
        erros_imprimir(erros);
        semantica_destruir(sem);
        return 1;
    }

    if (!type_checker_verificar(programa, sem) || erros_tem_erros(erros)) {
        erros_imprimir(erros);
        semantica_destruir(sem);
        return 1;
    }

    *out_sem = sem;
    return 0;
}

static int gerar_c(const char *arquivo_fonte,
                   No *programa,
                   ContextoSemantico *sem,
                   char **out_arquivo_c) {
    char *arquivo_c = substituir_extensao(arquivo_fonte, ".c");
    FILE *saida;

    if (!arquivo_c) {
        fprintf(stderr, "Falha ao montar caminho de saida C\n");
        return 1;
    }

    saida = fopen(arquivo_c, "w");
    if (!saida) {
        fprintf(stderr, "Falha ao abrir '%s' para escrita: %s\n", arquivo_c, strerror(errno));
        free(arquivo_c);
        return 1;
    }

    if (gerador_emitir(programa, sem, saida, &(OpcoesGerador){ .arquivo_fonte = arquivo_fonte }) != 0) {
        fprintf(stderr, "Falha ao gerar codigo C\n");
        fclose(saida);
        free(arquivo_c);
        return 1;
    }

    if (fclose(saida) != 0) {
        fprintf(stderr, "Falha ao fechar '%s'\n", arquivo_c);
        free(arquivo_c);
        return 1;
    }

    *out_arquivo_c = arquivo_c;
    return 0;
}

static void imprimir_token(const Token *token) {
    printf("%d:%d %-18s \"%s\"\n",
           token->loc.linha,
           token->loc.coluna,
           token_nome(token->tipo),
           token->lexema ? token->lexema : "");
}

static int comando_tokens(const char *arquivo_fonte) {
    char *fonte = ler_arquivo(arquivo_fonte);
    ContextoErros *erros;
    Lexer lexer;
    Token token;

    if (!fonte) {
        fprintf(stderr, "Falha ao ler '%s': %s\n", arquivo_fonte, strerror(errno));
        return 1;
    }

    erros = erros_criar();
    if (!erros) {
        free(fonte);
        fprintf(stderr, "Falha ao criar contexto de erros\n");
        return 1;
    }

    lexer_init(&lexer, fonte, arquivo_fonte, erros);
    do {
        TipoToken tipo_atual;
        token = lexer_proximo(&lexer);
        imprimir_token(&token);
        tipo_atual = token.tipo;
        token_destruir(&token);
        if (tipo_atual == TOKEN_EOF) {
            break;
        }
    } while (1);

    if (erros_tem_erros(erros)) {
        erros_imprimir(erros);
        erros_destruir(erros);
        free(fonte);
        return 1;
    }

    erros_destruir(erros);
    free(fonte);
    return 0;
}

static int comando_ast(const char *arquivo_fonte) {
    char *fonte = NULL;
    ContextoErros *erros = NULL;
    No *programa = NULL;
    int rc;

    rc = carregar_programa(arquivo_fonte, &fonte, &erros, &programa);
    if (rc == 0) {
        no_imprimir(programa, 0);
    }

    no_destruir(programa);
    erros_destruir(erros);
    free(fonte);
    tipos_singletons_destruir();
    return rc;
}

static int comando_fmt(const char *arquivo_fonte) {
    char *fonte = NULL;
    char *normalizada;
    size_t len;
    size_t i;
    size_t j = 0;
    int precisa_nova_linha;
    int rc;
    ContextoErros *erros = NULL;
    No *programa = NULL;

    rc = carregar_programa(arquivo_fonte, &fonte, &erros, &programa);
    if (rc != 0) {
        tipos_singletons_destruir();
        return rc;
    }

    len = strlen(fonte);
    normalizada = malloc(len + 2);
    if (!normalizada) {
        fprintf(stderr, "Falha ao alocar buffer para formatacao\n");
        no_destruir(programa);
        erros_destruir(erros);
        free(fonte);
        tipos_singletons_destruir();
        return 1;
    }

    for (i = 0; i < len; i++) {
        if (fonte[i] != '\r') {
            normalizada[j++] = fonte[i];
        }
    }

    while (j > 0 && (normalizada[j - 1] == '\n' || normalizada[j - 1] == ' ' || normalizada[j - 1] == '\t')) {
        if (normalizada[j - 1] == '\n') {
            break;
        }
        j--;
    }

    precisa_nova_linha = (j == 0 || normalizada[j - 1] != '\n');
    if (precisa_nova_linha) {
        normalizada[j++] = '\n';
    }
    normalizada[j] = '\0';

    rc = escrever_arquivo(arquivo_fonte, normalizada);
    if (rc != 0) {
        fprintf(stderr, "Falha ao escrever '%s'\n", arquivo_fonte);
    }

    free(normalizada);
    no_destruir(programa);
    erros_destruir(erros);
    free(fonte);
    tipos_singletons_destruir();
    return rc;
}

static int comando_init(void) {
    static const char *template_main =
        "function main() {\n"
        "\n"
        "    print(\"Hello World\")\n"
        "\n"
        "}\n";
    struct stat st;

    if (stat("main.ilc", &st) == 0) {
        fprintf(stderr, "Arquivo 'main.ilc' ja existe\n");
        return 1;
    }

    if (escrever_arquivo("main.ilc", template_main) != 0) {
        fprintf(stderr, "Falha ao criar 'main.ilc'\n");
        return 1;
    }

    printf("Projeto IluluC inicializado em main.ilc\n");
    return 0;
}

static int comando_ajuda(const char *argv0) {
    imprimir_uso(stdout, argv0);
    return 0;
}

static int comando_versao(void) {
    imprimir_versao();
    return 0;
}

static int comando_compilar(const char *arquivo_fonte, int executar_depois, int somente_c) {
    char *fonte = NULL;
    ContextoErros *erros = NULL;
    No *programa = NULL;
    ContextoSemantico *sem = NULL;
    char *arquivo_c = NULL;
    char *arquivo_saida = NULL;
    char *exec_path = NULL;
    int rc = 1;

    if (carregar_programa(arquivo_fonte, &fonte, &erros, &programa) != 0) {
        goto fim;
    }

    if (analisar_programa(programa, erros, &sem) != 0) {
        goto fim;
    }

    if (gerar_c(arquivo_fonte, programa, sem, &arquivo_c) != 0) {
        goto fim;
    }

    if (!somente_c || executar_depois) {
        arquivo_saida = nome_executavel(arquivo_fonte);
        if (!arquivo_saida) {
            fprintf(stderr, "Falha ao montar nome do executavel\n");
            goto fim;
        }

        if (compilar_c_para_executavel(arquivo_c, arquivo_saida) != 0) {
            fprintf(stderr, "Falha ao compilar '%s'\n", arquivo_c);
            goto fim;
        }
    }

    if (executar_depois) {
        char *argv_exec[2];
        exec_path = caminho_execucao(arquivo_saida);
        if (!exec_path) {
            fprintf(stderr, "Falha ao montar caminho de execucao\n");
            goto fim;
        }

        argv_exec[0] = exec_path;
        argv_exec[1] = NULL;
        if (executar_com_argv(argv_exec) != 0) {
            goto fim;
        }
    }

    rc = 0;

fim:
    free(exec_path);
    free(arquivo_saida);
    free(arquivo_c);
    semantica_destruir(sem);
    no_destruir(programa);
    erros_destruir(erros);
    free(fonte);
    tipos_singletons_destruir();
    return rc;
}

static int parsear_cli(int argc, char **argv, AcaoCLI *acao, const char **arquivo_fonte) {
    if (argc == 1) {
        *acao = ACAO_AJUDA;
        *arquivo_fonte = NULL;
        return 0;
    }

    if (argc == 2 && (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0)) {
        *acao = ACAO_AJUDA;
        *arquivo_fonte = NULL;
        return 0;
    }

    if (argc == 2 && (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "--version") == 0)) {
        *acao = ACAO_VERSAO;
        *arquivo_fonte = NULL;
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "init") == 0) {
        *acao = ACAO_INIT;
        *arquivo_fonte = NULL;
        return 0;
    }

    if (argc == 2 && nome_termina_com(argv[1], ".ilc")) {
        *acao = ACAO_COMPILAR;
        *arquivo_fonte = argv[1];
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "-c") == 0) {
        *acao = ACAO_GERAR_C;
        *arquivo_fonte = argv[2];
        return 0;
    }

    if (argc != 3) {
        return 1;
    }

    if (strcmp(argv[1], "comp") == 0) {
        *acao = ACAO_COMPILAR;
    } else if (strcmp(argv[1], "run") == 0) {
        *acao = ACAO_EXECUTAR;
    } else if (strcmp(argv[1], "c") == 0) {
        *acao = ACAO_GERAR_C;
    } else if (strcmp(argv[1], "ast") == 0) {
        *acao = ACAO_AST;
    } else if (strcmp(argv[1], "tokens") == 0) {
        *acao = ACAO_TOKENS;
    } else if (strcmp(argv[1], "fmt") == 0) {
        *acao = ACAO_FMT;
    } else {
        return 1;
    }

    *arquivo_fonte = argv[2];
    return 0;
}

int main(int argc, char **argv) {
    AcaoCLI acao;
    const char *arquivo_fonte;

    if (parsear_cli(argc, argv, &acao, &arquivo_fonte) != 0) {
        imprimir_uso(stderr, argv[0]);
        return 1;
    }

    if (acao != ACAO_AJUDA && acao != ACAO_VERSAO && acao != ACAO_INIT) {
        if (!arquivo_fonte || !nome_termina_com(arquivo_fonte, ".ilc")) {
            fprintf(stderr, "Esperado um arquivo .ilc\n");
            return 1;
        }
    }

    switch (acao) {
        case ACAO_AJUDA:
            return comando_ajuda(argv[0]);
        case ACAO_VERSAO:
            return comando_versao();
        case ACAO_COMPILAR:
            return comando_compilar(arquivo_fonte, 0, 0);
        case ACAO_EXECUTAR:
            return comando_compilar(arquivo_fonte, 1, 0);
        case ACAO_GERAR_C:
            return comando_compilar(arquivo_fonte, 0, 1);
        case ACAO_AST:
            return comando_ast(arquivo_fonte);
        case ACAO_TOKENS:
            return comando_tokens(arquivo_fonte);
        case ACAO_FMT:
            return comando_fmt(arquivo_fonte);
        case ACAO_INIT:
            return comando_init();
        default:
            imprimir_uso(stderr, argv[0]);
            return 1;
    }
}
