#include "btorconst.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

#ifndef NDEBUG

static int
valid_const (const char *c)
{
  const char *p;
  char ch;
  for (p = c; (ch = *p); p++)
    if (ch != '0' && ch != '1') return 0;
  return 1;
}

#endif

char *
btor_int_to_bin (BtorMemMgr *mm, int x, int len)
{
  char *result = NULL;
  int i        = 0;
  assert (mm != NULL);
  assert (x >= 0);
  assert (len > 0);
  result = (char *) btor_malloc (mm, sizeof (char) * (len + 1));
  for (i = len - 1; i >= 0; i--)
  {
    result[i] = x % 2 ? '1' : '0';
    x >>= 1;
  }
  result[len] = '\0';
  return result;
}

void
btor_delete_const (BtorMemMgr *mm, char *c)
{
  assert (valid_const (c));
  btor_freestr (mm, c);
}

char *
btor_slice_const (BtorMemMgr *mm, const char *a, int upper, int lower)
{
  const char *p, *eoa;
  char *res, *q;
  int len, delta;

  assert (valid_const (a));

  len = strlen (a);

  assert (0 <= lower);
  assert (upper <= len);

  delta = upper - lower + 1;

  res = btor_malloc (mm, delta + 1);

  p   = a + len - upper;
  q   = res;
  eoa = a + len - lower + 1;

  while (p < eoa) *q++ = *p++;

  *q = 0;

  assert (strlen (res) == (size_t) delta);

  return res;
}

static const char *
strip_zeroes (const char *a)
{
  while (*a == '0') a++;

  return a;
}

char *
btor_add_unbounded_const (BtorMemMgr *mm, const char *a, const char *b)
{
  char *res, *r, c, x, y, s;
  int alen, blen, rlen;
  const char *p, *q;

  assert (valid_const (a));
  assert (valid_const (b));

  a = strip_zeroes (a);
  b = strip_zeroes (b);

  if (!*a) return btor_strdup (mm, b);

  if (!*b) return btor_strdup (mm, a);

  alen = strlen (a);
  blen = strlen (b);
  rlen = (alen < blen) ? blen : alen;
  rlen++;

  res = btor_malloc (mm, rlen + 1);

  p = a + alen;
  q = b + blen;

  c = '0';

  r  = res + rlen;
  *r = 0;

  while (res < r)
  {
    x    = (a < p) ? *--p : '0';
    y    = (b < q) ? *--q : '0';
    s    = x ^ y ^ c;
    c    = (x & y) | (x & c) | (y & c);
    *--r = s;
  }

  return res;
}

char *
btor_mult_unbounded_const (BtorMemMgr *mm, const char *a, const char *b)
{
  char *res, *r, c, x, y, s, m;
  int alen, blen, rlen, i;
  const char *p;

  assert (valid_const (a));
  assert (valid_const (b));

  a = strip_zeroes (a);

  if (!*a) return btor_strdup (mm, "");

  if (a[0] == '1' && !a[1]) return btor_strdup (mm, b);

  b = strip_zeroes (b);

  if (!*b) return btor_strdup (mm, "");

  if (b[0] == '1' && !b[1]) return btor_strdup (mm, a);

  alen      = strlen (a);
  blen      = strlen (b);
  rlen      = alen + blen;
  res       = btor_malloc (mm, rlen + 1);
  res[rlen] = 0;

  for (r = res; r < res + blen; r++) *r = '0';

  for (p = a; p < a + alen; p++) *r++ = *p;

  assert (r == res + rlen);

  for (i = 0; i < alen; i++)
  {
    m = res[rlen - 1];
    c = '0';

    if (m == '1')
    {
      p = b + blen;
      r = res + blen;

      while (res < r)
      {
        x  = *--p;
        y  = *--r;
        s  = x ^ y ^ c;
        c  = (x & y) | (x & c) | (y & c);
        *r = s;
      }
    }

    memmove (res + 1, res, rlen - 1);
    res[0] = c;
  }

  return res;
}

int
btor_cmp_const (const char *a, const char *b)
{
  const char *p, *q, *s;
  int l, k, delta;

  assert (valid_const (a));
  assert (valid_const (b));

  a = strip_zeroes (a);
  b = strip_zeroes (b);

  l = strlen (a);
  k = strlen (b);

  delta = (l - k);

  if (delta < 0)
  {
    p = a;
    s = b - delta;

    for (q = b; q < s; q++)
      if (*q == '1') return -1;
  }
  else
  {
    s = a + delta;
    q = b;

    for (p = a; p < s; p++)
      if (*p == '1') return 1;
  }

  assert (strlen (p) == strlen (q));

  return strcmp (p, q);
}

char *
btor_sub_unbounded_const (BtorMemMgr *mem, const char *a, const char *b)
{
  char *res, *tmp, *r, c, x, y, s;
  int alen, blen, rlen;
  const char *p, *q;

  assert (valid_const (a));
  assert (valid_const (b));
  assert (btor_cmp_const (b, a) <= 0);

  a = strip_zeroes (a);
  b = strip_zeroes (b);
  if (!*b) return btor_strdup (mem, a);

  alen = strlen (a);
  blen = strlen (b);

  assert (alen >= blen);
  rlen = alen;

  res = btor_malloc (mem, rlen + 1);

  p = a + alen;
  q = b + blen;

  c  = '0';
  r  = res + rlen;
  *r = 0;

  while (res < r)
  {
    assert (a < p);
    x = *--p;

    y = (b < q) ? *--q : '0';

    s = x ^ y ^ c;
    c = ((1 ^ x) & c) | ((1 ^ x) & y) | (y & c);

    *--r = s;
  }

  assert (c == '0');

#ifndef NDEBUG
  {
    tmp = btor_add_unbounded_const (mem, res, b);
    assert (!btor_cmp_const (tmp, a));
    btor_freestr (mem, tmp);
  }
#endif

  tmp = btor_strdup (mem, strip_zeroes (res));
  btor_freestr (mem, res);
  res = tmp;

  return res;
}

char *
btor_div_unbounded_const (BtorMemMgr *mem,
                          const char *dividend,
                          const char *divisor,
                          char **rest_ptr)
{
  char *quotient, *rest, *r, *tmp;
  const char *p, *q;
  int len, qlen, rlen;

  assert (valid_const (dividend));
  assert (valid_const (divisor));

  dividend = strip_zeroes (dividend);
  divisor  = strip_zeroes (divisor);

  for (p = dividend; *p && *p == '0'; p++)
    ;

  for (q = divisor; *q && *q == '0'; q++)
    ;

  assert (*q); /* in any case even if 'dividend == 0' */

  if (!*p || btor_cmp_const (p, q) < 0) return btor_strdup (mem, "");

  len  = strlen (p);
  rlen = strlen (q);
  assert (rlen > 0);
  rlen--;
  assert (len > rlen);
  qlen = len - rlen;

  assert (*p); /* see above */
  assert (*q); /* see above */

  r = quotient = btor_malloc (mem, qlen + 1);
  rest         = btor_strdup (mem, p);

  while (r < quotient + qlen)
  {
  }

  assert (strlen (rest) <= (unsigned) rlen);
  assert (btor_cmp_const (rest, divisor) < 0);
#ifndef NDEBUG
  {
    char *tmp1 = btor_mult_unbounded_const (mem, quotient, divisor);
    char *tmp2 = btor_add_unbounded_const (mem, tmp1, rest);
    assert (!btor_cmp_const (dividend, tmp2));
    btor_freestr (mem, tmp1);
    btor_freestr (mem, tmp2);
  }
#endif
  if (rest_ptr) *rest_ptr = btor_strdup (mem, strip_zeroes (rest));

  btor_freestr (mem, rest);

  return quotient;
}

char *
btor_not_const (BtorMemMgr *mm, const char *a)
{
  char *result = NULL;
  int len      = 0;
  int i        = 0;
  assert (mm != NULL);
  assert (a != NULL);
  assert (strlen (a) > 0);
  assert (valid_const (a));
  len    = (int) strlen (a);
  result = (char *) btor_malloc (mm, sizeof (char) * (len + 1));
  for (i = len - 1; i >= 0; i--) result[i] = a[i] ^ 1;
  result[len] = '\0';
  return result;
}

char *
btor_and_const (BtorMemMgr *mm, const char *a, const char *b)
{
  char *result = NULL;
  int len      = 0;
  int i        = 0;
  assert (mm != NULL);
  assert (a != NULL);
  assert (b != NULL);
  assert (strlen (a) == strlen (b));
  assert (strlen (a) > 0);
  assert (valid_const (a));
  assert (valid_const (b));
  len    = (int) strlen (a);
  result = (char *) btor_malloc (mm, sizeof (char) * (len + 1));
  for (i = len - 1; i >= 0; i--) result[i] = a[i] & b[i];
  result[len] = '\0';
  return result;
}

char *
btor_eq_const (BtorMemMgr *mm, const char *a, const char *b)
{
  char *result = NULL;
  int len      = 0;
  int i        = 0;
  assert (mm != NULL);
  assert (a != NULL);
  assert (b != NULL);
  assert (strlen (a) == strlen (b));
  assert (strlen (a) > 0);
  assert (valid_const (a));
  assert (valid_const (b));
  len       = (int) strlen (a);
  result    = (char *) btor_malloc (mm, sizeof (char) * 2);
  result[0] = '1';
  for (i = len - 1; i >= 0; i--)
  {
    if (a[i] != b[i])
    {
      result[0] = '0';
      break;
    }
  }
  result[1] = '\0';
  return result;
}

char *
btor_add_const (BtorMemMgr *mm, const char *a, const char *b)
{
  char *result = NULL;
  char carry   = '0';
  int len      = 0;
  int i        = 0;
  assert (mm != NULL);
  assert (a != NULL);
  assert (b != NULL);
  assert (strlen (a) == strlen (b));
  assert (strlen (a) > 0);
  assert (valid_const (a));
  assert (valid_const (b));
  len    = (int) strlen (a);
  result = (char *) btor_malloc (mm, sizeof (char) * (len + 1));
  for (i = len - 1; i >= 0; i--)
  {
    result[i] = a[i] ^ b[i] ^ carry;
    carry     = (a[i] & b[i]) | (a[i] & carry) | (b[i] & carry);
  }
  result[len] = '\0';
  return result;
}

char *
btor_neg_const (BtorMemMgr *mm, const char *a)
{
  char *result = NULL;
  char *not_a  = NULL;
  char *one    = NULL;
  int len      = 0;
  assert (mm != NULL);
  assert (a != NULL);
  assert (strlen (a) > 0);
  assert (valid_const (a));
  len    = (int) strlen (a);
  not_a  = btor_not_const (mm, a);
  one    = btor_int_to_bin (mm, 1, len);
  result = btor_add_const (mm, not_a, one);
  btor_delete_const (mm, not_a);
  btor_delete_const (mm, one);
  return result;
}

char *
btor_sub_const (BtorMemMgr *mm, const char *a, const char *b)
{
  char *result = NULL;
  char *neg_b  = NULL;
  int len      = 0;
  assert (mm != NULL);
  assert (a != NULL);
  assert (b != NULL);
  assert (strlen (a) == strlen (b));
  assert (strlen (a) > 0);
  assert (valid_const (a));
  assert (valid_const (b));
  len    = (int) strlen (a);
  neg_b  = btor_neg_const (mm, b);
  result = btor_add_const (mm, a, neg_b);
  btor_delete_const (mm, neg_b);
  return result;
}

static char *
sll_n_bits (BtorMemMgr *mm, const char *a, int n)
{
  char *result = NULL;
  int len      = 0;
  int i        = 0;
  assert (mm != NULL);
  assert (a != NULL);
  assert (valid_const (a));
  assert (n >= 0);
  assert (n < (int) strlen (a));
  len = (int) strlen (a);
  if (len == 0) return btor_strdup (mm, a);
  result = (char *) btor_malloc (mm, sizeof (char) * (len + 1));
  for (i = 0; i < len - n; i++) result[i] = a[i + n];
  for (i = len - n; i < len; i++) result[i] = '0';
  result[len] = '\0';
  return result;
}

char *
btor_umul_const (BtorMemMgr *mm, const char *a, const char *b)
{
  char *result = NULL;
  char *and    = NULL;
  char *add    = NULL;
  char *shift  = NULL;
  int i        = 0;
  int j        = 0;
  int len      = 0;
  assert (mm != NULL);
  assert (a != NULL);
  assert (b != NULL);
  assert (strlen (a) == strlen (b));
  assert (strlen (a) > 0);
  assert (valid_const (a));
  assert (valid_const (b));
  len    = (int) strlen (a);
  result = btor_int_to_bin (mm, 0, len);
  for (i = len - 1; i >= 0; i--)
  {
    and = (char *) btor_malloc (mm, sizeof (char) * (len + 1));
    for (j = 0; j < len; j++) and[j] = a[j] & b[i];
    and[len] = '\0';
    shift    = sll_n_bits (mm, and, len - 1 - i);
    add      = btor_add_const (mm, result, shift);
    btor_delete_const (mm, result);
    btor_delete_const (mm, and);
    btor_delete_const (mm, shift);
    result = add;
  }
  return result;
}

static char *
srl_n_bits (BtorMemMgr *mm, const char *a, int n)
{
  char *result = NULL;
  int len      = 0;
  int i        = 0;
  assert (mm != NULL);
  assert (a != NULL);
  assert (valid_const (a));
  assert (n >= 0);
  assert (n < (int) strlen (a));
  len = (int) strlen (a);
  if (len == 0) return btor_strdup (mm, a);
  result = (char *) btor_malloc (mm, sizeof (char) * (len + 1));
  for (i = 0; i < n; i++) result[i] = '0';
  for (i = n; i < len; i++) result[i] = a[i - n];
  result[len] = '\0';
  return result;
}

static void
udiv_umod_const (BtorMemMgr *mm,
                 const char *a,
                 const char *b,
                 char **quotient,
                 char **remainder)
{
  char *b_i          = NULL;
  char *temp         = NULL;
  char *sub          = NULL;
  char *remainder_2n = NULL;
  int len            = 0;
  int len_2n         = 0;
  int i              = 0;
  int gte            = 0;
  assert (mm != NULL);
  assert (a != NULL);
  assert (b != NULL);
  assert (quotient != NULL);
  assert (remainder != NULL);
  assert (strlen (a) == strlen (b));
  assert (strlen (a) > 0);
  assert (valid_const (a));
  assert (valid_const (b));
  len = (int) strlen (a);
  assert (len <= INT_MAX / 2);
  len_2n           = len << 1;
  *quotient        = (char *) btor_malloc (mm, sizeof (char) * (len + 1));
  (*quotient)[len] = '\0';
  b_i              = (char *) btor_malloc (mm, sizeof (char) * (len_2n + 1));
  b_i[len_2n]      = '\0';
  remainder_2n     = (char *) btor_malloc (mm, sizeof (char) * (len_2n + 1));
  remainder_2n[len_2n] = '\0';
  for (i = 0; i < len; i++)
  {
    b_i[i]          = b[i];
    remainder_2n[i] = '0';
  }
  for (i = len; i < len_2n; i++)
  {
    b_i[i]          = '0';
    remainder_2n[i] = a[i - len];
  }
  for (i = len - 1; i >= 0; i--)
  {
    temp = srl_n_bits (mm, b_i, 1);
    btor_delete_const (mm, b_i);
    b_i                      = temp;
    gte                      = btor_cmp_const (remainder_2n, b_i) >= 0;
    (*quotient)[len - 1 - i] = (char) (48 ^ gte);
    if (gte)
    {
      sub = btor_sub_const (mm, remainder_2n, b_i);
      btor_delete_const (mm, remainder_2n);
      remainder_2n = sub;
    }
  }
  btor_delete_const (mm, b_i);
  *remainder        = (char *) btor_malloc (mm, sizeof (char) * (len + 1));
  (*remainder)[len] = '\0';
  for (i = len; i < len_2n; i++) (*remainder)[i - len] = remainder_2n[i];
  btor_delete_const (mm, remainder_2n);
}

char *
btor_udiv_const (BtorMemMgr *mm, const char *a, const char *b)
{
  char *quotient  = NULL;
  char *remainder = NULL;
  assert (mm != NULL);
  assert (a != NULL);
  assert (b != NULL);
  assert (strlen (a) == strlen (b));
  assert (strlen (a) > 0);
  assert (valid_const (a));
  assert (valid_const (b));
  udiv_umod_const (mm, a, b, &quotient, &remainder);
  btor_delete_const (mm, remainder);
  return quotient;
}

char *
btor_umod_const (BtorMemMgr *mm, const char *a, const char *b)
{
  char *quotient  = NULL;
  char *remainder = NULL;
  assert (mm != NULL);
  assert (a != NULL);
  assert (b != NULL);
  assert (strlen (a) == strlen (b));
  assert (strlen (a) > 0);
  assert (valid_const (a));
  assert (valid_const (b));
  udiv_umod_const (mm, a, b, &quotient, &remainder);
  btor_delete_const (mm, quotient);
  return remainder;
}

char *
btor_ult_const (BtorMemMgr *mm, const char *a, const char *b)
{
  char *result = NULL;
  assert (mm != NULL);
  assert (a != NULL);
  assert (b != NULL);
  assert (strlen (a) == strlen (b));
  assert (strlen (a) > 0);
  assert (valid_const (a));
  assert (valid_const (b));
  result = (char *) btor_malloc (mm, sizeof (char) * 2);
  if (strcmp (a, b) == -1)
    result[0] = '1';
  else
    result[0] = '0';
  result[1] = '\0';
  return result;
}

char *
btor_concat_const (BtorMemMgr *mm, const char *a, const char *b)
{
  char *result = NULL;
  assert (mm != NULL);
  assert (a != NULL);
  assert (b != NULL);
  assert (strlen (a) > 0);
  assert (strlen (b) > 0);
  assert (valid_const (a));
  assert (valid_const (b));
  result =
      (char *) btor_malloc (mm, sizeof (char) * (strlen (a) + strlen (b) + 1));
  strcpy (result, a);
  strcat (result, b);
  return result;
}

char *
btor_cond_const (BtorMemMgr *mm,
                 const char *const_cond,
                 const char *const_if,
                 const char *const_else)
{
  char *result = NULL;
  int len      = 0;
  int i        = 0;
  assert (mm != NULL);
  assert (const_cond != NULL);
  assert (const_if != NULL);
  assert (const_else != NULL);
  assert (strlen (const_cond) == 1);
  assert (strlen (const_if) == strlen (const_else));
  assert (strlen (const_if) > 0);
  assert (valid_const (const_cond));
  assert (valid_const (const_if));
  assert (valid_const (const_else));
  len    = (int) strlen (const_if);
  result = (char *) btor_malloc (mm, sizeof (char) * (len + 1));
  if (const_cond[0] == '1')
    for (i = 0; i < len; i++) result[i] = const_if[i];
  else
    for (i = 0; i < len; i++) result[i] = const_else[i];
  result[len] = '\0';
  return result;
}
