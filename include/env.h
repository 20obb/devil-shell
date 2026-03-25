/*
 * DevilShell (dsh) — env.h
 * Environment variable management and $VAR expansion
 */

#ifndef DSH_ENV_H
#define DSH_ENV_H

#include "shell.h"

/*
 * Initialize environment from the inherited environ.
 */
void  env_init(void);

/*
 * Get the value of an environment variable.
 * Returns NULL if not set.
 */
const char *env_get(const char *name);

/*
 * Set an environment variable (name=value).
 * If value is NULL, the variable is removed.
 */
void  env_set(const char *name, const char *value);

/*
 * Remove an environment variable.
 */
void  env_unset(const char *name);

/*
 * Expand all $VAR and ${VAR} references in a string.
 * Also expands ~ at the start to $HOME.
 * Returns a new malloc'd string (caller frees).
 */
char *env_expand(const char *str);

/*
 * Print all environment variables to stdout.
 */
void  env_print_all(void);

#endif /* DSH_ENV_H */
