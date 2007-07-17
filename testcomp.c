#include "testcomp.h"
#include "btorexit.h"
#include "btormain.h"
#include "btorutil.h"
#include "testrunner.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <limits.h>
#include <stdio.h>

#define BTOR_TEST_COMP_TEMP_FILE_NAME "comp.tmp"

#define BTOR_TEST_COMP_U_LOW 1
#define BTOR_TEST_COMP_U_HIGH 8

#define BTOR_TEST_COMP_S_LOW 2
#define BTOR_TEST_COMP_S_HIGH 8

static int g_argc     = 3;
static char *g_argv[] = {"./boolector", "-q", BTOR_TEST_COMP_TEMP_FILE_NAME};

void
init_comp_tests (void)
{
  FILE *f = fopen (BTOR_TEST_COMP_TEMP_FILE_NAME, "w");
  assert (f != NULL);
  fclose (f);
}

static void
u_comp_test (int (*func) (int, int), const char *func_name, int low, int high)
{
  FILE *f      = NULL;
  int i        = 0;
  int j        = 0;
  int result   = 0;
  int num_bits = 0;
  int max      = 0;
  assert (func != NULL);
  assert (func_name != NULL);
  assert (low > 0);
  assert (low <= high);
  BtorExitCode exit_code = 0;
  for (num_bits = low; num_bits <= high; num_bits++)
  {
    max = btor_pow_2_util (num_bits);
    for (i = 0; i < max; i++)
    {
      for (j = 0; j < max; j++)
      {
        result = func (i, j);
        f      = fopen (BTOR_TEST_COMP_TEMP_FILE_NAME, "w");
        assert (f != NULL);
        fprintf (f, "1 %d constd %d\n", num_bits, i);
        fprintf (f, "2 %d constd %d\n", num_bits, j);
        fprintf (f, "3 1 %s 1 2\n", func_name);
        fprintf (f, "4 1 root 3\n");
        fclose (f);
        exit_code = btor_main (g_argc, g_argv);
        assert (exit_code == BTOR_SAT_EXIT || exit_code == BTOR_UNSAT_EXIT);
        if (exit_code == BTOR_SAT_EXIT)
          assert (result);
        else
        {
          assert (exit_code = BTOR_UNSAT_EXIT);
          assert (!result);
        }
      }
    }
  }
}

static void
s_comp_test (int (*func) (int, int), const char *func_name, int low, int high)
{
  FILE *f                = NULL;
  int i                  = 0;
  int j                  = 0;
  int const1_id          = 0;
  int const2_id          = 0;
  int result             = 0;
  int num_bits           = 0;
  int max                = 0;
  BtorExitCode exit_code = 0;
  assert (func != NULL);
  assert (func_name != NULL);
  assert (low > 1);
  assert (low <= high);
  for (num_bits = low; num_bits <= high; num_bits++)
  {
    max = btor_pow_2_util (num_bits - 1);
    for (i = -max; i < max; i++)
    {
      for (j = -max; j < max; j++)
      {
        result = func (i, j);
        f      = fopen (BTOR_TEST_COMP_TEMP_FILE_NAME, "w");
        assert (f != NULL);
        if (i < 0)
        {
          fprintf (f, "1 %d constd %d\n", num_bits, -i);
          fprintf (f, "2 %d neg 1\n", num_bits);
          const1_id = 2;
        }
        else
        {
          fprintf (f, "1 %d constd %d\n", num_bits, i);
          const1_id = 1;
        }
        if (j < 0)
        {
          fprintf (f, "%d %d constd %d\n", const1_id + 1, num_bits, -j);
          fprintf (f, "%d %d neg %d\n", const1_id + 2, num_bits, const1_id + 1);
          const2_id = const1_id + 2;
        }
        else
        {
          fprintf (f, "%d %d constd %d\n", const1_id + 1, num_bits, j);
          const2_id = const1_id + 1;
        }

        fprintf (f,
                 "%d 1 %s %d %d\n",
                 const2_id + 1,
                 func_name,
                 const1_id,
                 const2_id);
        fprintf (f, "%d 1 root %d\n", const2_id + 2, const2_id + 1);
        fclose (f);
        exit_code = btor_main (g_argc, g_argv);
        assert (exit_code == BTOR_SAT_EXIT || exit_code == BTOR_UNSAT_EXIT);
        if (exit_code == BTOR_SAT_EXIT)
          assert (result);
        else
        {
          assert (exit_code == BTOR_UNSAT_EXIT);
          assert (!result);
        }
      }
    }
  }
}

static int
eq (int x, int y)
{
  return x == y;
}

static int
ne (int x, int y)
{
  return x != y;
}

static int
lt (int x, int y)
{
  return x < y;
}

static int
lte (int x, int y)
{
  return x <= y;
}

static int
gt (int x, int y)
{
  return x > y;
}

static int
gte (int x, int y)
{
  return x >= y;
}

static void
test_eq_1_comp (void)
{
  u_comp_test (eq, "eq", BTOR_TEST_COMP_U_LOW, BTOR_TEST_COMP_U_HIGH);
}

static void
test_ne_1_comp (void)
{
  u_comp_test (ne, "ne", BTOR_TEST_COMP_U_LOW, BTOR_TEST_COMP_U_HIGH);
}

static void
test_ult_comp (void)
{
  u_comp_test (lt, "ult", BTOR_TEST_COMP_U_LOW, BTOR_TEST_COMP_U_HIGH);
}

static void
test_ulte_comp (void)
{
  u_comp_test (lte, "ulte", BTOR_TEST_COMP_U_LOW, BTOR_TEST_COMP_U_HIGH);
}

static void
test_ugt_comp (void)
{
  u_comp_test (gt, "ugt", BTOR_TEST_COMP_U_LOW, BTOR_TEST_COMP_U_HIGH);
}

static void
test_ugte_comp (void)
{
  u_comp_test (gte, "ugte", BTOR_TEST_COMP_U_LOW, BTOR_TEST_COMP_U_HIGH);
}

static void
test_eq_2_comp (void)
{
  s_comp_test (eq, "eq", BTOR_TEST_COMP_S_LOW, BTOR_TEST_COMP_S_HIGH);
}

static void
test_ne_2_comp (void)
{
  s_comp_test (ne, "ne", BTOR_TEST_COMP_S_LOW, BTOR_TEST_COMP_S_HIGH);
}

static void
test_slt_comp (void)
{
  s_comp_test (lt, "slt", BTOR_TEST_COMP_S_LOW, BTOR_TEST_COMP_S_HIGH);
}

static void
test_slte_comp (void)
{
  s_comp_test (lte, "slte", BTOR_TEST_COMP_S_LOW, BTOR_TEST_COMP_S_HIGH);
}

static void
test_sgt_comp (void)
{
  s_comp_test (gt, "sgt", BTOR_TEST_COMP_S_LOW, BTOR_TEST_COMP_S_HIGH);
}

static void
test_sgte_comp (void)
{
  s_comp_test (gte, "sgte", BTOR_TEST_COMP_S_LOW, BTOR_TEST_COMP_S_HIGH);
}

void
run_comp_tests (int argc, char **argv)
{
  BTOR_RUN_TEST (eq_1_comp);
  BTOR_RUN_TEST (ne_1_comp);
  BTOR_RUN_TEST (ult_comp);
  BTOR_RUN_TEST (ulte_comp);
  BTOR_RUN_TEST (ugt_comp);
  BTOR_RUN_TEST (ugte_comp);
  BTOR_RUN_TEST (eq_2_comp);
  BTOR_RUN_TEST (ne_2_comp);
  BTOR_RUN_TEST (slt_comp);
  BTOR_RUN_TEST (slte_comp);
  BTOR_RUN_TEST (sgt_comp);
  BTOR_RUN_TEST (sgte_comp);
}

void
finish_comp_tests (void)
{
  int result = remove (BTOR_TEST_COMP_TEMP_FILE_NAME);
  assert (result == 0);
}
