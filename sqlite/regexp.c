/*
 * Expressões Regulares no SQLite:
 *
 *    REGEXP_VERSION_INFO, REGEXP, IREGEXP (somente no GNU REGEX), REGEXP_MATCH,
 *    REGEXP_MATCH_COUNT, REGEXP_MATCH_POSITION
 *
 * Há suporte ao "GNU Regular Expressions" aka GNU REGEX conforme documentado em
 * https://www.gnu.org/software/libc/manual/html_node/Regular-Expressions.html e
 * alternativamente, suporte ao "Perl Compatible Regular Expressions" aka PCRE,
 * conforme documentado em http://pcre.org/pcre.txt .
 * O PCRE tem muito mais recursos que o GNU REGEX, mas o segundo é legado da GNU
 * e somente por isso é o default.
 * Todas as funções fazem uso da persistência de dados do SQLite que otimiza o
 * desempenho desde que a mesma expressão regular seja aplicada sequencialmente
 * pela função a varias strings, pois mantém em cache cada instância compilada
 * do analisador de expressões.
 *
 * Dependências:
 *
 *    pacotes libsqlite3-dev e libpcre3-dev para suporte alternativo a PCRE
 *
 * Compilação para suporte a GNU REGEX (default):
 *
 *    gcc regexp.c -Wall -fPIC -shared -lm -o regexp.so
 *
 * Compilação para suporte a PCRE:
 *
 *    gcc regexp.c -Wall -fPIC -shared -lm -lpcre -DPCRE -o regexp.so
 *
 * Uso em arquivos de inicialização ou sessões interativas:
 *
 *    .load "path_to_lib/regexp.so"
 *
 * ou como requisição SQLite:
 *
 *    select load_extension("path_to_lib/regexp.so");
*/

#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include <stdlib.h>
#include <string.h>

#ifdef PCRE

#include <pcre.h>

typedef struct cache_entry_s
{
  pcre *p;
  pcre_extra *e;
}
cache_entry_t;

static void release_cache_entry_t(void *ptr)
{
  pcre_free( ((cache_entry_t *) ptr)->p );
  pcre_free_study( ((cache_entry_t *) ptr)->e );
  sqlite3_free(ptr);
}

/*
 * Testa se alguma substring da string alvo corresponde a uma expressão regular
 * em conformidade com o padrão Perl Compatible Regular Expressions (aka PCRE).
 *
 * O primeiro argumento deve ser a expressão regular e a string alvo da pesquisa
 * o segundo.
 *
 * O valor retornado é um inteiro tal que; sucesso é 1 e fracasso é 0.
 *
 * Importante: A função supre o operador REGEXP mencionado na documentação
 *             do SQLite, tal que a expressão regular é o segundo operando.
*/
static void regexp(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  cache_entry_t *c;
  const char *re, *str, *err;
  char *err2;
  int r;

  re = (const char *) sqlite3_value_text(argv[0]);
  if (!re) {
    sqlite3_result_error(ctx, "no regexp", -1);
    return ;
  }

  str = (const char *) sqlite3_value_text(argv[1]);
  if (!str) {
    sqlite3_result_error(ctx, "no string", -1);
    return ;
  }

  c = sqlite3_get_auxdata(ctx, 0);
  if (!c) {
    c = (cache_entry_t *) sqlite3_malloc(sizeof(cache_entry_t));
    if (!c) {
      sqlite3_result_error_nomem(ctx);
      return ;
    }
    c->p = pcre_compile(re, 0, &err, &r, NULL);
    if (!c->p)
    {
      err2 = sqlite3_mprintf("%s: %s (offset %d)", re, err, r);
      sqlite3_result_error(ctx, err2, -1);
      sqlite3_free(err2);
      return ;
    }
    c->e = pcre_study(c->p, 0, &err);
    if (!c->e && err) {
      err2 = sqlite3_mprintf("%s: %s", re, err);
      sqlite3_result_error(ctx, err2, -1);
      sqlite3_free(err2);
      return ;
    }
    sqlite3_set_auxdata(ctx, 0, c, release_cache_entry_t);
  }

  r = pcre_exec(c->p, c->e, str, strlen(str), 0, 0, NULL, 0);
  if (r >= 0) {
    sqlite3_result_int(ctx, 1);
  } else if (r == PCRE_ERROR_NOMATCH || r == PCRE_ERROR_NULL) {
    sqlite3_result_int(ctx, 0);
  } else {
    err2 = sqlite3_mprintf("PCRE execution failed with code %d.", r);
    sqlite3_result_error(ctx, err2, -1);
    sqlite3_free(err2);
  }
}

/*
 * Retorna a primeira substring da string pesquisada que corresponder à
 * expressão regular ou NULL se a pesquisa for mal sucedida.
 *
 * O primeiro argumento deve ser a expressão regular e a string alvo da
 * pesquisa o segundo.
*/
static void regexp_match(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  cache_entry_t *c;
  const char *re, *err;
  char *err2, *str;
  int ovector[6];
  int r;

  re = (const char *) sqlite3_value_text(argv[0]);
  if (!re) {
    sqlite3_result_error(ctx, "no regexp", -1);
    return ;
  }

  str = (char *) sqlite3_value_text(argv[1]);
  if (!str) {
    sqlite3_result_error(ctx, "no string", -1);
    return ;
  }

  c = sqlite3_get_auxdata(ctx, 0);
  if (!c) {
    c = (cache_entry_t *) sqlite3_malloc(sizeof(cache_entry_t));
    if (!c) {
      sqlite3_result_error_nomem(ctx);
      return ;
    }
    c->p = pcre_compile(re, 0, &err, &r, NULL);
    if (!c->p)
    {
      err2 = sqlite3_mprintf("%s: %s (offset %d)", re, err, r);
      sqlite3_result_error(ctx, err2, -1);
      sqlite3_free(err2);
      return ;
    }
    c->e = pcre_study(c->p, 0, &err);
    if (!c->e && err) {
      err2 = sqlite3_mprintf("%s: %s", re, err);
      sqlite3_result_error(ctx, err2, -1);
      sqlite3_free(err2);
      return ;
    }
    sqlite3_set_auxdata(ctx, 0, c, release_cache_entry_t);
  }

  r = pcre_exec(c->p, c->e, str, strlen(str), 0, 0, ovector, 6);
  if (r >= 0)
  {
    *(str + ovector[1]) = '\0';
    sqlite3_result_text(ctx, str+ovector[0], -1, SQLITE_TRANSIENT);
  } else
  {
    switch (r) {
      case PCRE_ERROR_NOMATCH:
      case PCRE_ERROR_NULL:
        sqlite3_result_null(ctx);
        break;
      default:
        err2 = sqlite3_mprintf("PCRE execution failed with code %d.", r);
        sqlite3_result_error(ctx, err2, -1);
        sqlite3_free(err2);
        break;
    }
  }
}

/*
 * Retorna o número de substrings da string pesquisada que corresponderem à
 * expressão regular.
 *
 * O primeiro argumento deve ser a expressão regular e a string alvo da pesquisa
 * o segundo.
*/
static void regexp_match_count(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  cache_entry_t *c;
  const char *re, *str, *err;
  char *err2;
  int len, count, r;
  int *offset;
  int ovector[10];

  re = (const char *) sqlite3_value_text(argv[0]);
  if (!re) {
    sqlite3_result_error(ctx, "no regexp", -1);
    return ;
  }

  str = (const char *) sqlite3_value_text(argv[1]);
  if (!str) {
    sqlite3_result_error(ctx, "no string", -1);
    return ;
  }

  c = sqlite3_get_auxdata(ctx, 0);
  if (!c) {
    c = (cache_entry_t *) sqlite3_malloc(sizeof(cache_entry_t));
    if (!c) {
      sqlite3_result_error_nomem(ctx);
      return ;
    }
    c->p = pcre_compile(re, 0, &err, &r, NULL);
    if (!c->p)
    {
      err2 = sqlite3_mprintf("%s: %s (offset %d)", re, err, r);
      sqlite3_result_error(ctx, err2, -1);
      sqlite3_free(err2);
      return ;
    }
    c->e = pcre_study(c->p, 0, &err);
    if (!c->e && err) {
      err2 = sqlite3_mprintf("%s: %s", re, err);
      sqlite3_result_error(ctx, err2, -1);
      sqlite3_free(err2);
      return ;
    }
    sqlite3_set_auxdata(ctx, 0, c, release_cache_entry_t);
  }

  offset = ovector + 1;
  *offset = 0;
  len = strlen(str);
  count = 0;
  while ((r = pcre_exec(c->p, c->e, str, len, *offset, 0, ovector, 10)) >= 0) ++count;
  if (r == PCRE_ERROR_NOMATCH || r == PCRE_ERROR_NULL) {
    sqlite3_result_int(ctx, count);
  } else {
    err2 = sqlite3_mprintf("PCRE execution failed with code %d.", r);
    sqlite3_result_error(ctx, err2, -1);
    sqlite3_free(err2);
  }
}

/*
 * Retorna a posição, offset em bytes, de uma das substring que correspondem à
 * expressão regular conforme seu número de ordem.
 *
 * Se o número de ordem for menor igual a 0 ou maior que o número de substrings
 * identificadas será retornado -1.
 *
 * O primeiro argumento deve ser a expressão regular, a string alvo da pesquisa
 * o segundo e o número de ordem da substring o terceiro.
*/
static void regexp_match_position(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  cache_entry_t *c;
  const char *re, *str, *err;
  char *err2;
  int group, len, r;
  int *offset, *position;
  int ovector[10];

  re = (const char *) sqlite3_value_text(argv[0]);
  if (!re) {
    sqlite3_result_error(ctx, "no regexp", -1);
    return ;
  }

  str = (const char *) sqlite3_value_text(argv[1]);
  if (!str) {
    sqlite3_result_error(ctx, "no string", -1);
    return ;
  }

  group = sqlite3_value_int(argv[2]);
  if (group <= 0) {
    sqlite3_result_error(ctx, "matching substring order must to be > 0", -1);
    return ;
  }

  c = sqlite3_get_auxdata(ctx, 0);
  if (!c) {
    c = (cache_entry_t *) sqlite3_malloc(sizeof(cache_entry_t));
    if (!c) {
      sqlite3_result_error_nomem(ctx);
      return ;
    }
    c->p = pcre_compile(re, 0, &err, &r, NULL);
    if (!c->p)
    {
      err2 = sqlite3_mprintf("%s: %s (offset %d)", re, err, r);
      sqlite3_result_error(ctx, err2, -1);
      sqlite3_free(err2);
      return ;
    }
    c->e = pcre_study(c->p, 0, &err);
    if (!c->e && err) {
      err2 = sqlite3_mprintf("%s: %s", re, err);
      sqlite3_result_error(ctx, err2, -1);
      sqlite3_free(err2);
      return ;
    }
    sqlite3_set_auxdata(ctx, 0, c, release_cache_entry_t);
  }

  position = ovector;
  *position = -1;
  offset = ovector + 1;
  *offset = 0;
  len = strlen(str);
  while (group > 0) {
    r = pcre_exec(c->p, c->e, str, len, *offset, 0, ovector, 10);
    if (r < 0) break;
    --group;
  }
  if (r == PCRE_ERROR_NOMATCH || r == PCRE_ERROR_NULL || r >= 0) {
    sqlite3_result_int(ctx, (group > 0 ? -1 : *position));
  } else {
    err2 = sqlite3_mprintf("PCRE execution failed with code %d.", r);
    sqlite3_result_error(ctx, err2, -1);
    sqlite3_free(err2);
  }
}

#else /* GNU REGEX */

#include <regex.h>

/*
 * Testa se alguma substring da string alvo corresponde a uma expressão regular
 * em conformidade com o padrão GNU Regular Expressions no modo Extended.
 * A expressão regular trata letras maiúsculas como diferentes de minúsculas,
 * caracteres Unicode devem ser declarados explicitamente e se a expressão
 * regular for mal formada, será mostrada a mensagem de erro correspondente.
 *
 * O primeiro argumento deve ser a expressão regular e a string alvo da pesquisa
 * o segundo.
 *
 * O valor retornado é um inteiro tal que; sucesso é 1 e fracasso é 0.
 *
 * Importante: A função supre o operador REGEXP mencionado na documentação
 *             do SQLite, tal que a expressão regular é o segundo operando.
*/
static void regexp(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  regex_t *exp;
  const char *p, *z;
  char *err;
  int r, v;

  p = (const char *) sqlite3_value_text(argv[0]);
  z = (const char *) sqlite3_value_text(argv[1]);

  exp = sqlite3_get_auxdata(ctx, 0);
  if (!exp) {
    exp = sqlite3_malloc( sizeof(regex_t) );
    if (!exp) {
      sqlite3_result_error(ctx, "No room to compile expression.", -1);
      return ;
    }
    v = regcomp(exp, p, REG_EXTENDED | REG_NOSUB);
    if (v != 0) {
      r = regerror(v, exp, NULL, 0);
      err = (char *) sqlite3_malloc(r);
      (void) regerror(v, exp, err, r);
      sqlite3_result_error(ctx, err, -1);
      sqlite3_free(err);
      return ;
    }
    sqlite3_set_auxdata(ctx, 0, exp, sqlite3_free);
  }

  r = regexec(exp, z, 0, 0, 0);
  sqlite3_result_int(ctx, r == 0);
}

/*
 * Conforme anterior, porém não trata letras maiúsculas como diferentes de
 * minúsculas.
*/
static void iregexp(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  regex_t *exp;
  const char *p, *z;
  char *err;
  int r, v;

  p = (const char *) sqlite3_value_text(argv[0]);
  z = (const char *) sqlite3_value_text(argv[1]);

  exp = sqlite3_get_auxdata(ctx, 0);
  if (!exp) {
    exp = sqlite3_malloc( sizeof(regex_t) );
    if (!exp) {
      sqlite3_result_error(ctx, "No room to compile expression.", -1);
      return ;
    }
    v = regcomp(exp, p, REG_EXTENDED | REG_NOSUB | REG_ICASE);
    if (v != 0) {
      r = regerror(v, exp, NULL, 0);
      err = (char *) sqlite3_malloc(r);
      (void) regerror(v, exp, err, r);
      sqlite3_result_error(ctx, err, -1);
      sqlite3_free(err);
      return ;
    }
    sqlite3_set_auxdata(ctx, 0, exp, sqlite3_free);
  }

  r = regexec(exp, z, 0, 0, 0);
  sqlite3_result_int(ctx, r == 0);
}

#define MAX_MATCHES 1 /* número máximo de identificações numa string qualquer */

/*
 * Retorna a primeira substring da string pesquisada que corresponder à
 * expressão regular, que pode ser uma string vazia em caso de fracasso.
 *
 * O primeiro argumento deve ser a expressão regular e a string alvo da pesquisa
 * o segundo.
*/
static void regexp_match(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  regex_t *exp;
  const char *p, *z;
  char *err, *rz;
  int v, r;
  regmatch_t matches[MAX_MATCHES];

  p = (const char *) sqlite3_value_text(argv[0]);
  z = (const char *) sqlite3_value_text(argv[1]);

  exp = sqlite3_get_auxdata(ctx, 0);
  if (!exp) {
    exp = sqlite3_malloc( sizeof(regex_t) );
    if (!exp) {
      sqlite3_result_error(ctx, "No room to compile expression.", -1);
      return ;
    }
    v = regcomp(exp, p, REG_EXTENDED);
    if (v != 0) {
      r = regerror(v, exp, NULL, 0);
      err = (char *) sqlite3_malloc(r);
      (void) regerror(v, exp, err, r);
      sqlite3_result_error(ctx, err, -1);
      sqlite3_free(err);
      return ;
    }
    sqlite3_set_auxdata(ctx, 0, exp, sqlite3_free);
  }

  r = regexec(exp, z, MAX_MATCHES, matches, 0);
  if (r == 0) {
    v = matches[0].rm_eo - matches[0].rm_so;
    rz = (char *) sqlite3_malloc(v+1);
    memcpy(rz, z+matches[0].rm_so, v);
    *(rz + matches[0].rm_eo) = '\0';
    sqlite3_result_text(ctx, rz, -1, SQLITE_TRANSIENT);
    sqlite3_free(rz);
  }
}

/*
 * Retorna o número de substrings da string pesquisada que corresponderem à
 * expressão regular.
 *
 * O primeiro argumento deve ser a expressão regular e a string alvo da pesquisa
 * o segundo.
*/
static void regexp_match_count(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  regex_t *exp;
  const char *p, *z;
  char *err;
  int v, r;
  regmatch_t matches[MAX_MATCHES];

  p = (char *) sqlite3_value_text(argv[0]);
  z = (char *) sqlite3_value_text(argv[1]);

  exp = sqlite3_get_auxdata(ctx, 0);
  if (!exp) {
    exp = sqlite3_malloc( sizeof(regex_t) );
    if (!exp) {
      sqlite3_result_error(ctx, "No room to compile expression.", -1);
      return ;
    }
    v = regcomp(exp, p, REG_EXTENDED);
    if (v != 0) {
      r = regerror(v, exp, NULL, 0);
      err = (char *) sqlite3_malloc(r);
      (void) regerror(v, exp, err, r);
      sqlite3_result_error(ctx, err, -1);
      sqlite3_free(err);
      return ;
    }
    sqlite3_set_auxdata(ctx, 0, exp, sqlite3_free);
  }

  v = r = 0;
  while ( *(z+r) != '\0' && regexec(exp, z+r, MAX_MATCHES, matches, 0) == 0 )
  {
    r += matches[0].rm_eo;
    v++;
  }
  sqlite3_result_int(ctx, v);
}

/*
 * Retorna a posição de uma das substring que correspondem à expressão regular.
 *
 * O primeiro argumento deve ser a expressão regular, a string alvo da pesquisa
 * o segundo e o número de ordem da substring o terceiro.
 *
 * Se o número de ordem for 0 ou maior que o número de substrings identificadas
 * então será retornado o valor inteiro -1.
*/
static void regexp_match_position(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  regex_t *exp;
  const char *p, *z;
  char *err;
  int v, r, group;
  regmatch_t matches[MAX_MATCHES];

  p = (const char *) sqlite3_value_text(argv[0]);
  z = (const char *) sqlite3_value_text(argv[1]);
  group = sqlite3_value_int(argv[2]);

  exp = sqlite3_get_auxdata(ctx, 0);
  if (!exp) {
    exp = sqlite3_malloc( sizeof(regex_t) );
    if (!exp) {
      sqlite3_result_error(ctx, "No room to compile expression.", -1);
      return ;
    }
    v = regcomp(exp, p, REG_EXTENDED);
    if (v != 0) {
      r = regerror(v, exp, NULL, 0);
      err = (char *) sqlite3_malloc(r);
      (void) regerror(v, exp, err, r);
      sqlite3_result_error(ctx, err, -1);
      sqlite3_free(err);
      return ;
    }
    sqlite3_set_auxdata(ctx, 0, exp, sqlite3_free);
  }

  v = 0; r = -1;
  while ( *(z+v) != '\0' && group > 0
         && regexec(exp, z+v, MAX_MATCHES, matches, 0) == 0 )
  {
    r = v + matches[0].rm_so;
    v += matches[0].rm_eo;
    --group;
  }
  sqlite3_result_int(ctx, group == 0 ? r : -1);
}

#endif /* GNU Regex */

/* Informa a marca e versão da API de expressões regulares. */
static void regexp_version_info(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  char *z = sqlite3_mprintf(
#ifdef _PCRE_H
  "PCRE %d.%d", PCRE_MAJOR, PCRE_MINOR
#else
  "GNU REGEX part of GNU C Library %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__
#endif
  );
  sqlite3_result_text(ctx, z, -1, SQLITE_TRANSIENT);
  sqlite3_free(z);
}

int sqlite3_extension_init(sqlite3 *db, char **err, const sqlite3_api_routines *api)
{
  SQLITE_EXTENSION_INIT2(api)

  sqlite3_create_function(db, "REGEXP_VERSION_INFO",  0, SQLITE_UTF8, NULL, regexp_version_info, NULL, NULL);
  sqlite3_create_function(db, "REGEXP", 2, SQLITE_UTF8, NULL, regexp, NULL, NULL);
#ifdef _REGEX_H
  sqlite3_create_function(db, "IREGEXP", 2, SQLITE_UTF8, NULL, iregexp, NULL, NULL);
#endif
  sqlite3_create_function(db, "REGEXP_MATCH", 2, SQLITE_UTF8, NULL, regexp_match, NULL, NULL);
  sqlite3_create_function(db, "REGEXP_MATCH_COUNT", 2, SQLITE_UTF8, NULL, regexp_match_count, NULL, NULL);
  sqlite3_create_function(db, "REGEXP_MATCH_POSITION",  3, SQLITE_UTF8, NULL, regexp_match_position, NULL, NULL);

  return 0;
}
