/*
 * type_checker.h - Verificação de tipos do IluluC
 *
 * Segunda passagem: infere e valida tipos em expressões e statements,
 * preenchendo no->tipo_inferido em cada nó.
 */

#ifndef ILULUC_TYPE_CHECKER_H
#define ILULUC_TYPE_CHECKER_H

#include "ast.h"
#include "semantic.h"

/* Executa verificação de tipos sobre o programa */
int type_checker_verificar(No *programa, ContextoSemantico *ctx);

#endif /* ILULUC_TYPE_CHECKER_H */
