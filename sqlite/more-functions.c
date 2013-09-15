/*
 * Math: power
 *
 * String: reverse, zeropad
 *
 * Bitwise: int2bin, bitstatus
 *
 * Bitwise aggregation: group_bitor, group_ndxbitor
 *
 * Regular Expressions: regexp, iregexp (only in PCRE), regexp_match,
 *                      regexp_match_count, regexp_match_position
 *
 * Miscellaneous: mask60, quadrante, datalocal, datefield, chkdate, rownum
 *
 * Compile: gcc more-functions.c -fPIC -shared -lm -lpcre -o more-functions.so
 *
 * Usage: .load "path_to_lib/more-functions.so"
 * or also for JDBC: select load_extension("path_to_lib/more-functions.so");
*/
#define COMPILE_SQLITE_EXTENSIONS_AS_LOADABLE_MODULE 1

#ifdef COMPILE_SQLITE_EXTENSIONS_AS_LOADABLE_MODULE
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#else
#include "sqlite3.h"
#endif

#include <assert.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef int64_t   i64;

#include <math.h>
#include <errno.h>		/* LMH 2007-03-25 */

/*
 * Wraps the pow math.h function
*/
static void powerFunc(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  double r1 = 0.0;
  double r2 = 0.0;
  double val;

  assert( argc==2 );

  if (sqlite3_value_type(argv[0]) == SQLITE_NULL
      || sqlite3_value_type(argv[1]) == SQLITE_NULL) {
    sqlite3_result_null(context);
  } else {
    r1 = sqlite3_value_double(argv[0]);
    r2 = sqlite3_value_double(argv[1]);
    errno = 0;
    val = pow(r1,r2);
    if (errno == 0) {
      sqlite3_result_double(context, val);
    } else {
      sqlite3_result_error(context, strerror(errno), errno);
    }
  }
}

#include <limits.h>

#define I64_NBITS (sizeof(i64) * CHAR_BIT)

static char *int2bin(i64 n, char *buf)
{
  char *t;
  for (t = buf+I64_NBITS, *t = '\0'; t != buf; n >>= 1) *(--t) = (n & 1) | '0';
  return buf;
}

/*
 * Returns the binary representation string of an integer up to 64 bits.
*/
static void int2binFunc(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  char *buffer;
  i64 iVal;

  assert( 1 == argc );

  if ( SQLITE_INTEGER == sqlite3_value_type(argv[0]) ) {
    iVal = sqlite3_value_int64(argv[0]);
    buffer = sqlite3_malloc( I64_NBITS+1 );
    if (!buffer) {
      sqlite3_result_error_nomem(context);
    } else {
      int2bin(iVal, buffer);
      sqlite3_result_text(context, buffer, -1, SQLITE_TRANSIENT);
      sqlite3_free(buffer);
    }
  } else {
    sqlite3_result_error(context, "invalid type", -1);
  }
}

#define N_DEZENAS 60 /* quantidade de números da Mega-Sena */

/*
 * Monta máscara de incidência dos números da Mega-Sena agrupados via
 * bitwise OR no único argumento de tipo inteiro.
*/
static void mask60Func(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  char *buffer;
  i64 iVal;
  int i;

  assert( 1 == argc );

  if ( SQLITE_INTEGER == sqlite3_value_type(argv[0]) ) {
    iVal = sqlite3_value_int64(argv[0]);
    if (iVal < 0) {
      sqlite3_result_error(context, "argumento é negativo", -1);
    } else {
      buffer = (char *) sqlite3_malloc( N_DEZENAS+1 );
      if (!buffer) {
        sqlite3_result_error_nomem(context);
      } else {
        for (i=0; i < N_DEZENAS; i++, iVal >>= 1) buffer[i] = (iVal & 1) | '0';
        buffer[N_DEZENAS] = '\0';
        sqlite3_result_text(context, buffer, -1, SQLITE_TRANSIENT);
        sqlite3_free(buffer);
      }
    }
  } else {
    sqlite3_result_error(context, "tipo do argumento é invalido", -1);
  }
}

/*
 * Retorna o quadrante do número da Mega-Sena conforme apresentado no boleto.
*/
static void quadranteFunc(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  int d, q;

  assert( 1 == argc );

  if ( SQLITE_INTEGER == sqlite3_value_type(argv[0]) ) {
    d = sqlite3_value_int(argv[0]);
    if (d < 1 || d > N_DEZENAS) {
      sqlite3_result_error(context, "argumento é menor que 1 ou maior que 60", -1);
      return;
    }
    q = ((d-1) / 20 + 1) * 10 + (((d-1) % 10) / 2 + 1);
    sqlite3_result_int(context, q);
  } else {
    sqlite3_result_error(context, "argumento não é do tipo inteiro", -1);
    return;
  }
}

/*
 * Returns the bit status of an integer up to 64 bits as first argument
 * and the zero-based bit number as second argument.
*/
static void bitstatusFunc(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  i64 iVal;
  int bitNumber;

  assert( 2 == argc );

  if ( SQLITE_INTEGER == sqlite3_value_type(argv[0]) ) {
    iVal = sqlite3_value_int64(argv[0]);
    if ( SQLITE_INTEGER == sqlite3_value_type(argv[1]) ) {
      bitNumber = sqlite3_value_int(argv[1]);
      if (bitNumber >= 0 && bitNumber < I64_NBITS) {
        sqlite3_result_int(context, (iVal >> bitNumber) & 1);
      } else {
        sqlite3_result_error(context, "error: bit number isn't in [0;63]", -1);
      }
    } else {
      sqlite3_result_error(context, "error: bit status 2nd argument isn't an integer", -1);
    }
  } else {
    sqlite3_result_error(context, "error: bit status 1st argument isn't an integer", -1);
  }
}

typedef struct BitCtx {
  i64 rB;
}
BitCtx;

/*
 * returns the resulting value of bitwise OR on group itens
*/
static void group_bitorFinalize(sqlite3_context *context)
{
  BitCtx *p;

  p = sqlite3_aggregate_context(context, sizeof(BitCtx));
  sqlite3_result_int64(context, p->rB);
}

/*
 * Acumula o resultado do BITWISE OR entre o valor da estrutura de contexto
 * e o argumento inteiro a cada iteração da função de agregação de valores
 * agrupados.
*/
static void group_bitorStep(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  BitCtx *p;
  i64 iVal;

  assert( 1 == argc );

  if ( SQLITE_INTEGER == sqlite3_value_numeric_type(argv[0]) ) {
    iVal = sqlite3_value_int64(argv[0]);
    p = sqlite3_aggregate_context(context, sizeof(BitCtx));
    p->rB |= iVal;
  } else {
    sqlite3_result_error(context, "error: BITOR argument isn't an integer", -1);
  }
}

/*
 * BITWISE OR dos índices dos números da Mega-Sena.
*/
static void group_ndxbitorStep(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  BitCtx *p;
  int iVal;

  assert( 1 == argc );

  if ( SQLITE_INTEGER == sqlite3_value_numeric_type(argv[0]) ) {
    iVal = sqlite3_value_int(argv[0]);
    if (iVal > 0 && iVal <= N_DEZENAS) {
      p = sqlite3_aggregate_context(context, sizeof(BitCtx));
      p->rB |= ((i64) 1) << (iVal-1);
    } else {
      sqlite3_result_error(context, "argumento é menor que 1 ou maior que 60", -1);
    }
  } else {
    sqlite3_result_error(context, "argumento nao é do tipo inteiro", -1);
  }
}

/* LMH from sqlite3 3.3.13
 *
 * This table maps from the first byte of a UTF-8 character to the number
 * of trailing bytes expected. A value '4' indicates that the table key
 * is not a legal first byte for a UTF-8 character.
*/
static const u8 xtra_utf8_bytes[256]  = {
  /* 0xxxxxxx */
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,

  /* 10wwwwww */
  4, 4, 4, 4, 4, 4, 4, 4,     4, 4, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 4, 4, 4, 4,     4, 4, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 4, 4, 4, 4,     4, 4, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 4, 4, 4, 4,     4, 4, 4, 4, 4, 4, 4, 4,

  /* 110yyyyy */
  1, 1, 1, 1, 1, 1, 1, 1,     1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,     1, 1, 1, 1, 1, 1, 1, 1,

  /* 1110zzzz */
  2, 2, 2, 2, 2, 2, 2, 2,     2, 2, 2, 2, 2, 2, 2, 2,

  /* 11110yyy */
  3, 3, 3, 3, 3, 3, 3, 3,     4, 4, 4, 4, 4, 4, 4, 4,
};

/*
 * This table maps from the number of trailing bytes in a UTF-8 character
 * to an integer constant that is effectively calculated for each character
 * read by a naive implementation of a UTF-8 character reader. The code
 * in the READ_UTF8 macro explains things best.
*/
static const int xtra_utf8_bits[] =  {
  0,
  12416,          /* (0xC0 << 6)  + (0x80) */
  925824,         /* (0xE0 << 12) + (0x80 << 6) + (0x80) */
  63447168        /* (0xF0 << 18) + (0x80 << 12) + (0x80 << 6) + 0x80 */
};

/*
 * If a UTF-8 character contains N bytes extra bytes (N bytes follow
 * the initial byte so that the total character length is N+1) then
 * masking the character with utf8_mask[N] must produce a non-zero
 * result.  Otherwise, we have an (illegal) overlong encoding.
*/
static const int utf_mask[] = {
  0x00000000,
  0xffffff80,
  0xfffff800,
  0xffff0000,
};

static int sqlite3ReadUtf8(const unsigned char *z)
{
  int c;
  /* LMH salvaged from sqlite3 3.3.13 source code src/utf.c */
  // READ_UTF8(z, c);
  int xtra;
  c = *(z)++;
  xtra = xtra_utf8_bytes[c];
  switch (xtra) {
    case 4: c = (int) 0xFFFD; break;
    case 3: c = (c << 6) + *(z)++;
    case 2: c = (c << 6) + *(z)++;
    case 1: c = (c << 6) + *(z)++;
    c -= xtra_utf8_bits[xtra];
    if ((utf_mask[xtra] & c) == 0
        || (c&0xFFFFF800) == 0xD800
        || (c&0xFFFFFFFE) == 0xFFFE ) { c = 0xFFFD; }
  }
  return c;
}

/*
 * X is a pointer to the first byte of a UTF-8 character.  Increment
 * X so that it points to the next character.  This only works right
 * if X points to a well-formed UTF-8 string.
*/
#define sqliteNextChar(X)  while( (0xC0 & *(++X)) == 0x80 ){}
#define sqliteCharVal(X)   sqlite3ReadUtf8(X)

/*
 * Returns the source string with the characters in reverse order.
*/
static void reverseFunc(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  unsigned char *z, *t;
  char *rz, *r;
  int n;

  assert( 1 == argc );

  if ( SQLITE_NULL == sqlite3_value_type(argv[0]) )
  {
    sqlite3_result_null(context);
    return;
  }
  t = z = (unsigned char *) sqlite3_value_text(argv[0]);
  n = strlen((char *) z);
  r = rz = (char *) sqlite3_malloc(n + 1);
  if (!rz)
  {
    sqlite3_result_error_nomem(context);
    return;
  }
  *(rz += n) = '\0';
  while (sqliteCharVal(t) != 0)
  {
    z = t;
    sqliteNextChar(t);
    rz -= n = t - z;
    memcpy(rz, z, n);
  }

  assert(r == rz);

  sqlite3_result_text(context, rz, -1, SQLITE_TRANSIENT);
  sqlite3_free(rz);
}

/*
 * Returns a left zero padded string of the first positive int argument with
 * minimum length corresponding to the second positive int argument.
*/
static void zeropadFunc(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  i64 iVal;
  int iSize, j;
  char *z, *format;

  assert( argc == 2 );

  for (j=0; j < argc; j++) {
    switch( sqlite3_value_type(argv[j]) ) {
      case SQLITE_INTEGER: {
        if (j == 0)
          iVal = sqlite3_value_int64(argv[0]);
        else
          iSize = sqlite3_value_int(argv[1]);
        break;
      }
      case SQLITE_NULL: {
        sqlite3_result_null(context);
        break;
      }
      default: {
        sqlite3_result_error(context, "invalid type", -1);
        break;
      }
    }
  }
  if (iVal < 0 || iSize < 0) {
    sqlite3_result_error(context, "domain error", -1);
    return;
  }
  format = sqlite3_mprintf("%%0%dd", iSize);
  z = sqlite3_mprintf(format, iVal);
  sqlite3_free(format);
  sqlite3_result_text(context, z, -1, SQLITE_TRANSIENT);
  sqlite3_free(z);
}

#include <stdlib.h>

/*
 * Validação de data conforme ISO 8601 usando data no formato YYYY-MM-DD
 * e também checa os valores dos componentes da data.
*/
static int chkdate(const char *date)
{
  char *t = (char *) date;
  int j;

  // identificação do formato 'YYYY-MM-DD'

  for (j=0; *t && j < 10; j++, t++)
  {
    if (j != 4 && j != 7) {
      if (*t < '0' || *t > '9') return 0;
    } else {
      if (*t != '-') return 0;
    }
  }
  if (*t || j != 10) return 0;

  // extrai e valida componentes da data

  int days_in_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  int year, month, day;

  t = (char *) sqlite3_malloc(4+1);
  memcpy(t, date+5, 2);
  *(t+2) = '\0';
  month = atoi(t);
  j = (month > 0 && month <= 12);
  if (j) {
    if (month == 2)
    {
      memcpy(t, date, 4);
      *(t+4) = '\0';
      year = atoi(t);
      days_in_month[1] = (year%4 == 0 && year%100 != 0) || year%400 == 0 ? 29 : 28;
    }
    memcpy(t, date+8, 2);
    *(t+2) = '\0';
    day = atoi(t);
    j = (day > 0 && day <= days_in_month[month-1]);
  }
  sqlite3_free(t);

  return j;
}

/*
 * Validate a date string against ISO 8601 using date format YYYY-MM-DD
 * and also check-up the date fields values.
*/
static void chkdateFunc(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  const char *z;

  assert(1 == argc);

  if ( SQLITE_TEXT != sqlite3_value_type(argv[0]) ) {
    sqlite3_result_error(ctx, "argument isn't a string", -1);
    return;
  }
  z = (const char *) sqlite3_value_text(argv[0]);

  sqlite3_result_int(ctx, chkdate(z));
}

/*
 * Extract field from a datestring in first argument with index order in
 * second argument where 0 is for year, 1 is for month and 2 is for day.
*/
static void datefieldFunc(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  const int NDX[] = { 0, 5, 8 }; /* fields offset indexes */
  const int LEN[] = { 4, 2, 2 }; /* fields lengths */
  char *z;
  int f;

  assert( 2 == argc );

  /* check if first argument type is text */
  if ( SQLITE_TEXT != sqlite3_value_type(argv[0]) ) {
    sqlite3_result_error(context, "1st argument isn't a string", -1);
    return;
  }
  z = (char *) sqlite3_value_text(argv[0]);
  /**/
  if (!chkdate(z)) {
    sqlite3_result_error(context, "wrong date string", -1);
    return;
  }
  /* check if second argument type is integer */
  if ( SQLITE_INTEGER != sqlite3_value_type(argv[1]) ) {
    sqlite3_result_error(context, "2nd argument isn't an integer", -1);
    return;
  }
  f = sqlite3_value_int(argv[1]);
  /* check field offset index argument value */
  if (f < 0 || f > 2) {
    sqlite3_result_error(context, "2nd argument domain error", -1);
    return;
  }

  *(z + NDX[f] + LEN[f]) = '\0';

  sqlite3_result_text(context, z+NDX[f], -1, SQLITE_TRANSIENT);
}

#define WORD(ptr) *((unsigned short int *) (ptr))

#define SWAP(a, b) WORD(a) ^= WORD(b), WORD(b) ^= WORD(a), WORD(a) ^= WORD(b)

/*
 * Retorna a data 'year-mM-dD' no formato 'dD-mM-year'.
*/
static void datalocalFunc(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  char *date;

  assert( 1 == argc );

  date = (char *) sqlite3_value_text(argv[0]);   // year-mM-dD

  if (!chkdate(date)) {
    sqlite3_result_error(ctx, "data invalida", -1);
    return ;
  }

  /*
    memmove(&int, date, 4);      // salva 'year'                   ????-mM-dD
    memmove(date, date+8, 2);    // move 'dD' p/início             dd??-mM-??
    memmove(date+2, date+4, 4);  // move '-mM-' 2 bytes a frente   dD-mM-????
    memmove(date+6, &int, 4);    // restaura 'year'                dD-mM-year
  */

  // código alternativo independente de funções

  SWAP(date,   date+8);  // dDar-mM-ye
  SWAP(date+4, date+2);  // dD-marM-ye
  SWAP(date+6, date+4);  // dD-mM-arye
  SWAP(date+8, date+6);  // dD-mM-year

  sqlite3_result_text(ctx, date, -1, SQLITE_TRANSIENT);
}

/*
 * The ROWNUM code was borrowed from: http://sqlite.1065341.n5.nabble.com/sequential-row-numbers-from-query-td47370.html
*/

typedef struct ROWNUM_t ROWNUM_t;
struct ROWNUM_t {
  int nNumber;
};

static void rownum_free(void *p)
{
  sqlite3_free(p);
}

/*
 * Retorna o número da linha na tabela, necessariamente usando como argumento
 * qualquer valor constante.
*/
static void rownumFunc(sqlite3_context *context, int argc, sqlite3_value **argv
)
{
  ROWNUM_t *pAux;

  pAux = sqlite3_get_auxdata(context, 0);
  if (!pAux) {
    pAux = (ROWNUM_t *) sqlite3_malloc( sizeof(ROWNUM_t) );
    if (pAux) {
      pAux->nNumber = 0;
      sqlite3_set_auxdata(context, 0, (void *) pAux, rownum_free);
    } else {
      sqlite3_result_error(context, "sqlite3_malloc failed", -1);
      return;
    }
  }
  pAux->nNumber++;

  sqlite3_result_int(context, pAux->nNumber);
}

#ifdef PCRE

/*
 * Suporte a Perl Compatible Regular Expressions (aka PCRE) conforme documentado
 * em http://pcre.org/pcre.txt
*/

#include <pcre.h>

typedef struct cache_entry {
  pcre *p;
  pcre_extra *e;
}
cache_entry;

static void release_cache_entry(void *ptr)
{
  pcre_free( ((cache_entry *) ptr)->p );
  pcre_free_study( ((cache_entry *) ptr)->e );
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
 * Importante: A função supre o operador REGEXP mencionado na documentação.
*/
static void regexp(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  cache_entry *c;
  const char *re, *str, *err;
  char *err2;
  int r;

  assert(argc == 2);

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
    c = (cache_entry *) sqlite3_malloc(sizeof(cache_entry));
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
    sqlite3_set_auxdata(ctx, 0, c, release_cache_entry);
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
  cache_entry *c;
  const char *re, *err;
  char *err2, *str;
  int ovector[6];
  int r;

  assert(argc == 2);

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
    c = (cache_entry *) sqlite3_malloc(sizeof(cache_entry));
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
    sqlite3_set_auxdata(ctx, 0, c, release_cache_entry);
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
  cache_entry *c;
  const char *re, *str, *err;
  char *err2;
  int len, count, r;
  int *offset;
  int ovector[10];

  assert(argc == 2);

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
    c = (cache_entry *) sqlite3_malloc(sizeof(cache_entry));
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
    sqlite3_set_auxdata(ctx, 0, c, release_cache_entry);
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
  cache_entry *c;
  const char *re, *str, *err;
  char *err2;
  int group, len, r;
  int *offset, *position;
  int ovector[10];

  assert(argc == 3);

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
    c = (cache_entry *) sqlite3_malloc(sizeof(cache_entry));
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
    sqlite3_set_auxdata(ctx, 0, c, release_cache_entry);
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

#else /* GNU Regex */

/*
 * Suporte a GNU Regular Expressions (aka GNU Regex) conforme documentado em:
 * https://www.gnu.org/software/libc/manual/html_node/Regular-Expressions.html
*/

#include <sys/types.h>
#include <regex.h>

/*
 * Testa se alguma substring da string alvo corresponde a uma expressão regular
 * em conformidade com o padrão GNU Regular Expressions no modo Extended.
 * A expressão regular considera letras maiúsculas como diferentes de minúsculas
 * , caracteres Unicode devem ser declarados explicitamente e se a expressão
 * regular for mal formada, será mostrada a mensagem de erro correspondente.
 *
 * O primeiro argumento deve ser a expressão regular e a string alvo da pesquisa
 * o segundo.
 *
 * O valor retornado é um inteiro tal que; sucesso é 1 e fracasso é 0.
 *
 * Importante: A função supre o operador REGEXP mencionado na documentação.
*/
static void regexp(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  regex_t *exp;
  const char *p, *z;
  char *err;
  int r, v;

  assert(argc == 2);

  p = (const char *) sqlite3_value_text(argv[0]);
  z = (const char *) sqlite3_value_text(argv[1]);

  exp = sqlite3_get_auxdata(context, 0);
  if (!exp) {
    exp = sqlite3_malloc( sizeof(regex_t) );
    if (!exp) {
      sqlite3_result_error(context, "No room to compile expression.", -1);
      return ;
    }
    v = regcomp(exp, p, REG_EXTENDED | REG_NOSUB);
    if (v != 0) {
      r = regerror(v, exp, NULL, 0);
      err = (char *) sqlite3_malloc(r);
      (void) regerror(v, exp, err, r);
      sqlite3_result_error(context, err, -1);
      sqlite3_free(err);
      return ;
    }
    sqlite3_set_auxdata(context, 0, exp, sqlite3_free);
  }

  r = regexec(exp, z, 0, 0, 0);
  sqlite3_result_int(context, r == 0);
}

/*
 * Conforme anterior, porém não considera maiúsculas diferentes de minúsculas.
*/
static void iregexp(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  regex_t *exp;
  const char *p, *z;
  char *err;
  int r, v;

  assert(argc == 2);

  p = (const char *) sqlite3_value_text(argv[0]);
  z = (const char *) sqlite3_value_text(argv[1]);

  exp = sqlite3_get_auxdata(context, 0);
  if (!exp) {
    exp = sqlite3_malloc( sizeof(regex_t) );
    if (!exp) {
      sqlite3_result_error(context, "No room to compile expression.", -1);
      return ;
    }
    v = regcomp(exp, p, REG_EXTENDED | REG_NOSUB | REG_ICASE);
    if (v != 0) {
      r = regerror(v, exp, NULL, 0);
      err = (char *) sqlite3_malloc(r);
      (void) regerror(v, exp, err, r);
      sqlite3_result_error(context, err, -1);
      sqlite3_free(err);
      return ;
    }
    sqlite3_set_auxdata(context, 0, exp, sqlite3_free);
  }

  r = regexec(exp, z, 0, 0, 0);
  sqlite3_result_int(context, r == 0);
}

#define MAX_MATCHES 1 /* número máximo de identificações numa string qualquer */

/*
 * Retorna a primeira substring da string pesquisada que corresponder à
 * expressão regular, que pode ser uma string vazia em caso de fracasso.
 *
 * O primeiro argumento deve ser a expressão regular e a string alvo da pesquisa
 * o segundo.
*/
static void regexp_match(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  regex_t *exp;
  const char *p, *z;
  char *err, *rz;
  int v, r;
  regmatch_t matches[MAX_MATCHES];

  assert(argc == 2);

  p = (const char *) sqlite3_value_text(argv[0]);
  z = (const char *) sqlite3_value_text(argv[1]);

  exp = sqlite3_get_auxdata(context, 0);
  if (!exp) {
    exp = sqlite3_malloc( sizeof(regex_t) );
    if (!exp) {
      sqlite3_result_error(context, "No room to compile expression.", -1);
      return ;
    }
    v = regcomp(exp, p, REG_EXTENDED);
    if (v != 0) {
      r = regerror(v, exp, NULL, 0);
      err = (char *) sqlite3_malloc(r);
      (void) regerror(v, exp, err, r);
      sqlite3_result_error(context, err, -1);
      sqlite3_free(err);
      return ;
    }
    sqlite3_set_auxdata(context, 0, exp, sqlite3_free);
  }

  r = regexec(exp, z, MAX_MATCHES, matches, 0);
  if (r == 0) {
    v = matches[0].rm_eo - matches[0].rm_so;
    rz = (char *) sqlite3_malloc(v+1);
    memcpy(rz, z+matches[0].rm_so, v);
    *(rz + matches[0].rm_eo) = '\0';
    sqlite3_result_text(context, rz, -1, SQLITE_TRANSIENT);
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
static void regexp_match_count(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  regex_t *exp;
  const char *p, *z;
  char *err;
  int v, r;
  regmatch_t matches[MAX_MATCHES];

  assert(argc == 2);

  p = (char *) sqlite3_value_text(argv[0]);
  z = (char *) sqlite3_value_text(argv[1]);

  exp = sqlite3_get_auxdata(context, 0);
  if (!exp) {
    exp = sqlite3_malloc( sizeof(regex_t) );
    if (!exp) {
      sqlite3_result_error(context, "No room to compile expression.", -1);
      return ;
    }
    v = regcomp(exp, p, REG_EXTENDED);
    if (v != 0) {
      r = regerror(v, exp, NULL, 0);
      err = (char *) sqlite3_malloc(r);
      (void) regerror(v, exp, err, r);
      sqlite3_result_error(context, err, -1);
      sqlite3_free(err);
      return ;
    }
    sqlite3_set_auxdata(context, 0, exp, sqlite3_free);
  }

  v = r = 0;
  while ( *(z+r) != '\0' && regexec(exp, z+r, MAX_MATCHES, matches, 0) == 0 )
  {
    r += matches[0].rm_eo;
    v++;
  }
  sqlite3_result_int(context, v);
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
static void regexp_match_position(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  regex_t *exp;
  const char *p, *z;
  char *err;
  int v, r, group;
  regmatch_t matches[MAX_MATCHES];

  assert(argc == 3);

  p = (const char *) sqlite3_value_text(argv[0]);
  z = (const char *) sqlite3_value_text(argv[1]);
  group = sqlite3_value_int(argv[2]);

  exp = sqlite3_get_auxdata(context, 0);
  if (!exp) {
    exp = sqlite3_malloc( sizeof(regex_t) );
    if (!exp) {
      sqlite3_result_error(context, "No room to compile expression.", -1);
      return ;
    }
    v = regcomp(exp, p, REG_EXTENDED);
    if (v != 0) {
      r = regerror(v, exp, NULL, 0);
      err = (char *) sqlite3_malloc(r);
      (void) regerror(v, exp, err, r);
      sqlite3_result_error(context, err, -1);
      sqlite3_free(err);
      return ;
    }
    sqlite3_set_auxdata(context, 0, exp, sqlite3_free);
  }

  v = 0; r = -1;
  while ( *(z+v) != '\0' && group > 0
         && regexec(exp, z+v, MAX_MATCHES, matches, 0) == 0 )
  {
    r = v + matches[0].rm_so;
    v += matches[0].rm_eo;
    --group;
  }
  sqlite3_result_int(context, group == 0 ? r : -1);
}

#endif /* GNU Regex */

/*
 * This function registered all of the above C functions as SQL
 * functions.  This should be the only routine in this file with
 * external linkage.
*/
int RegisterExtensionFunctions(sqlite3 *db)
{
  static const struct FuncDef {
     char *zName;
     signed char nArg;
     u8 argType;           /* 0: none.  1: db  2: (-1) */
     u8 eTextRep;          /* 1: UTF-16.  0: UTF-8 */
     u8 needCollSeq;
     void (*xFunc)(sqlite3_context*,int,sqlite3_value **);
  } aFuncs[] = {

    { "power",              2, 0, SQLITE_UTF8,    0, powerFunc  },

    { "reverse",            1, 0, SQLITE_UTF8,    0, reverseFunc },
    { "zeropad",            2, 0, SQLITE_UTF8,    0, zeropadFunc },

    /* bitwise */
    { "int2bin",            1, 0, SQLITE_UTF8,    0, int2binFunc },
    { "bitstatus",          2, 0, SQLITE_UTF8,    0, bitstatusFunc },

    { "mask60",             1, 0, SQLITE_UTF8,    0, mask60Func },
    { "quadrante",          1, 0, SQLITE_UTF8,    0, quadranteFunc },

    { "datefield",          2, 0, SQLITE_UTF8,    0, datefieldFunc },
    { "datalocal",          1, 0, SQLITE_UTF8,    0, datalocalFunc },
    { "chkdate",            1, 0, SQLITE_UTF8,    0, chkdateFunc },

    { "rownum",             1, 0, SQLITE_UTF8,    0, rownumFunc },

    { "regexp",                 2, 0, SQLITE_UTF8, 0, regexp },
#ifndef PCRE
    { "iregexp",                2, 0, SQLITE_UTF8, 0, iregexp },
#endif
    { "regexp_match",           2, 0, SQLITE_UTF8, 0, regexp_match },
    { "regexp_match_count",     2, 0, SQLITE_UTF8, 0, regexp_match_count },
    { "regexp_match_position",  3, 0, SQLITE_UTF8, 0, regexp_match_position },

  };

  /* Aggregate functions */
  static const struct FuncDefAgg {
    char *zName;
    signed char nArg;
    u8 argType;
    u8 needCollSeq;
    void (*xStep)(sqlite3_context*,int,sqlite3_value**);
    void (*xFinalize)(sqlite3_context*);
  } aAggs[] = {

    { "group_bitor",      1, 0, 0, group_bitorStep, group_bitorFinalize },
    { "group_ndxbitor",   1, 0, 0, group_ndxbitorStep, group_bitorFinalize },

  };

  int i;
  for (i=0; i<sizeof(aFuncs)/sizeof(aFuncs[0]); i++) {
    void *pArg = 0;
    switch ( aFuncs[i].argType ) {
      case 1: pArg = db; break;
      case 2: pArg = (void *)(-1); break;
    }
    //sqlite3CreateFunc
    /* LMH no error checking */
    sqlite3_create_function(db, aFuncs[i].zName, aFuncs[i].nArg,
        aFuncs[i].eTextRep, pArg, aFuncs[i].xFunc, 0, 0);
#if 0
    if ( aFuncs[i].needCollSeq ) {
      struct FuncDef *pFunc = sqlite3FindFunction(db, aFuncs[i].zName,
          strlen(aFuncs[i].zName), aFuncs[i].nArg, aFuncs[i].eTextRep, 0);
      if ( pFunc && aFuncs[i].needCollSeq ) {
        pFunc->needCollSeq = 1;
      }
    }
#endif
  }

  for (i=0; i<sizeof(aAggs)/sizeof(aAggs[0]); i++) {
    void *pArg = 0;
    switch ( aAggs[i].argType ) {
      case 1: pArg = db; break;
      case 2: pArg = (void *)(-1); break;
    }
    //sqlite3CreateFunc
    /* LMH no error checking */
    sqlite3_create_function(db, aAggs[i].zName, aAggs[i].nArg, SQLITE_UTF8,
        pArg, 0, aAggs[i].xStep, aAggs[i].xFinalize);
#if 0
    if ( aAggs[i].needCollSeq ) {
      struct FuncDefAgg *pFunc = sqlite3FindFunction( db, aAggs[i].zName,
          strlen(aAggs[i].zName), aAggs[i].nArg, SQLITE_UTF8, 0);
      if ( pFunc && aAggs[i].needCollSeq ) {
        pFunc->needCollSeq = 1;
      }
    }
#endif
  }
  return 0;
}

#ifdef COMPILE_SQLITE_EXTENSIONS_AS_LOADABLE_MODULE
int sqlite3_extension_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi)
{
  SQLITE_EXTENSION_INIT2(pApi);
  RegisterExtensionFunctions(db);
  return 0;
}
#endif /* COMPILE_SQLITE_EXTENSIONS_AS_LOADABLE_MODULE */
