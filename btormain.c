#include "btormain.h"
#include "btorexit.h"
#include "btorftor.h"
#include "btorsat.h"
#include "btorutil.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *g_usage =
    "usage: boolector [<option>...][<input>]\n"
    "\n"
    "where <option> is one of the following:"
    "\n"
    "  -h|--help               print usage information and exit\n"
    "  -v|--verbose            increase verbosity\n"
    "  -V|--version            print version and exit\n"
    "  -de|--dump-exp <file>   dump expression in BAF\n"
    "  -da|--dump-aig <file>   dump AIG in AIGER\n"
    "  -dc|--dump-cnf <file>   dump CNF in DIMACS\n"
    "  -o <output>             set output file\n"
    "  -q|--quiet              do not print any output\n"
    "  -c|--credits            print credits\n";

static const char *g_credits =
    "**************************\n"
    "*       BOOLECTOR        *\n"
    "**************************\n"
    "* by Robert D. Brummayer *\n"
    "*          FMV           *\n"
    "*    JKU Linz Austria    *\n"
    "**************************\n"
    "*      Contributors:     *\n"
    "**************************\n"
    "*        Armin Biere     *\n"
    "**************************";

int g_quiet;
FILE *g_output_file;

static void
print_msg (char *msg)
{
  assert (msg != NULL);
  if (!g_quiet) fprintf (g_output_file, msg);
}

static void
print_msg_va_args (char *msg, ...)
{
  va_list list;
  assert (msg != NULL);
  if (!g_quiet)
  {
    va_start (list, msg);
    vfprintf (g_output_file, msg, list);
    va_end (list);
  }
}

int
btor_main (int argc, char **argv)
{
  int sat_result               = 0;
  int done                     = 0;
  int err                      = 0;
  int i                        = 0;
  int j                        = 0;
  int close_input_file         = 0;
  int close_output_file        = 0;
  int close_exp_file           = 0;
  int close_aig_file           = 0;
  int close_cnf_file           = 0;
  int dump_exp                 = 0;
  int dump_aig                 = 0;
  int dump_cnf                 = 0;
  int verbosity                = 0;
  const char *input_file_name  = "<stdin>";
  const char *output_file_name = "<stdout>";
  const char *exp_file_name    = NULL;
  const char *aig_file_name    = NULL;
  const char *cnf_file_name    = NULL;
  const char *parse_error      = NULL;
  char *witness                = NULL;
  FILE *file                   = NULL;
  FILE *input_file             = stdin;
  FILE *exp_file               = stdout;
  FILE *aig_file               = stdout;
  FILE *cnf_file               = stdout;
  BtorExpMgr *emgr             = NULL;
  BtorExp *cur_exp             = NULL;
  BtorAIGMgr *amgr             = NULL;
  BtorAIG *aig                 = NULL;
  BtorFtorResult ftor_res;
  BtorFtor *ftor = NULL;

  g_quiet       = 0;
  g_output_file = stdout;

  for (i = 1; !done && !err && i < argc; i++)
  {
    if (!strcmp (argv[i], "-h") || !strcmp (argv[i], "--help"))
    {
      print_msg_va_args ("%s\n", g_usage);
      done = 1;
    }
    else if (!strcmp (argv[i], "-c") || !strcmp (argv[i], "--credits"))
    {
      print_msg_va_args ("%s\n", g_credits);
      done = 1;
    }
    else if (!strcmp (argv[i], "-de") || !strcmp (argv[i], "--dump-exp"))
    {
      dump_exp = 1;
      if (i < argc - 1)
      {
        if (close_exp_file)
        {
          fclose (exp_file);
          close_exp_file = 0;
          print_msg_va_args ("boolector: multiple expression files\n");
          err = 1;
        }
        else
        {
          exp_file = fopen (argv[++i], "w");
          if (exp_file == NULL)
          {
            print_msg_va_args ("boolector: can not create '%s'\n", argv[i]);
            err = 1;
          }
          else
          {
            exp_file_name  = argv[i];
            close_exp_file = 1;
          }
        }
      }
    }
    else if (!strcmp (argv[i], "-da") || !strcmp (argv[i], "--dump-aig"))
    {
      dump_aig = 1;
      if (i < argc - 1)
      {
        if (close_aig_file)
        {
          fclose (aig_file);
          close_aig_file = 0;
          print_msg_va_args ("boolector: multiple AIG files\n");
          err = 1;
        }
        else
        {
          aig_file = fopen (argv[++i], "w");
          if (aig_file == NULL)
          {
            print_msg_va_args ("boolector: can not create '%s'\n", argv[i]);
            err = 1;
          }
          else
          {
            aig_file_name  = argv[i];
            close_aig_file = 1;
          }
        }
      }
    }
    else if (!strcmp (argv[i], "-dc") || !strcmp (argv[i], "--dump-cnf"))
    {
      dump_cnf = 1;
      if (i < argc - 1)
      {
        if (close_cnf_file)
        {
          fclose (cnf_file);
          close_cnf_file = 0;
          print_msg_va_args ("boolector: multiple CNF files\n");
          err = 1;
        }
        else
        {
          cnf_file = fopen (argv[++i], "w");
          if (cnf_file == NULL)
          {
            print_msg_va_args ("boolector: can not create '%s'\n", argv[i]);
            err = 1;
          }
          else
          {
            cnf_file_name  = argv[i];
            close_cnf_file = 1;
          }
        }
      }
    }
    else if (!strcmp (argv[i], "-v") || !strcmp (argv[i], "--verbose"))
    {
      verbosity++;
    }
    else if (!strcmp (argv[i], "-V") || !strcmp (argv[i], "--version"))
    {
      print_msg_va_args ("%s\n", BTOR_VERSION);
      done = 1;
    }
    else if (!strcmp (argv[i], "-q") || !strcmp (argv[i], "--quiet"))
    {
      g_quiet = 1;
    }
    else if (!strcmp (argv[i], "-o"))
    {
      if (i < argc - 1)
      {
        if (close_output_file)
        {
          fclose (g_output_file);
          close_output_file = 0;
          g_output_file     = stdout;
          print_msg_va_args ("boolector: multiple output files\n");
          err = 1;
        }
        else
        {
          g_output_file = fopen (argv[++i], "w");
          if (g_output_file == NULL)
          {
            g_output_file = stdout;
            print_msg_va_args ("boolector: can not create '%s'\n", argv[i]);
            err = 1;
          }
          else
          {
            output_file_name  = argv[i];
            close_output_file = 1;
          }
        }
      }
    }
    else if (argv[i][0] == '-')
    {
      print_msg_va_args ("boolector: invalid option '%s'\n", argv[i]);
      err = 1;
    }
    else if (close_input_file)
    {
      print_msg_va_args ("boolector: multiple input files\n");
      err = 1;
    }
    else if (!(file = fopen (argv[i], "r")))
    {
      print_msg_va_args ("boolector: can not read '%s'\n", argv[i]);
      err = 1;
    }
    else
    {
      input_file_name  = argv[i];
      input_file       = file;
      close_input_file = 1;
    }
  }

  if (!done && !err)
  {
    emgr = btor_new_exp_mgr ();
    ftor = btor_new_ftor (emgr);

    parse_error =
        btor_parse_ftor (ftor, input_file, input_file_name, &ftor_res);

    if (parse_error)
    {
      print_msg_va_args ("%s\n", parse_error);
      err = 1;
    }
    else if (ftor_res.nroots != 1)
    {
      print_msg_va_args ("%s: found %d roots but expected exactly one\n",
                         input_file_name,
                         ftor_res.nroots);
      err = 1;
    }
    else if (dump_exp || dump_aig || dump_cnf)
    {
      done = 1;
      if (dump_exp)
      {
        btor_dump_exp (emgr, exp_file, ftor_res.roots[0]);
      }
      if (dump_aig || dump_cnf)
      {
        amgr = btor_get_aig_mgr_aigvec_mgr (btor_get_aigvec_mgr_exp_mgr (emgr));
        aig  = btor_exp_to_aig (emgr, ftor_res.roots[0]);
        if (dump_aig) btor_dump_aig (amgr, aig_file, aig);
        if (dump_cnf)
        {
          btor_init_sat ();
          btor_aig_to_sat (amgr, aig);
          btor_dump_cnf_sat (cnf_file);
          btor_reset_sat ();
        }
        btor_release_aig (amgr, aig);
      }
    }
    else
    {
      btor_init_sat ();
      sat_result = btor_sat_exp (emgr, ftor_res.roots[0]);
      if (sat_result == BTOR_UNSAT)
        print_msg ("UNSATISFIABLE\n");
      else if (sat_result == BTOR_SAT)
      {
        print_msg ("SATISFIABLE\n");
        for (i = 0; i < ftor_res.nvars; i++)
        {
          cur_exp = ftor_res.vars[i];
          witness = btor_get_assignment_var_exp (emgr, cur_exp);
          if (witness != NULL)
            print_msg_va_args (
                "%s: %s\n", btor_get_symbol_exp (emgr, cur_exp), witness);
        }
        for (i = 0; i < ftor_res.narrays; i++)
        {
          cur_exp = ftor_res.arrays[i];
          assert (!BTOR_IS_INVERTED_EXP (cur_exp));
          for (j = 0; j < btor_pow_2_util (cur_exp->index_len); j++)
          {
            witness = btor_get_assignment_array_exp (emgr, cur_exp, j);
            if (witness != NULL)
              print_msg_va_args ("%s[%d]: %s\n",
                                 btor_get_symbol_exp (emgr, cur_exp),
                                 j,
                                 witness);
          }
        }
      }
      else
      {
        print_msg ("UNKNOWN SAT RESULT\n");
      }
      btor_reset_sat ();
    }
    btor_delete_ftor (ftor);
    btor_delete_exp_mgr (emgr);
  }

  if (close_input_file) fclose (input_file);
  if (close_output_file) fclose (g_output_file);
  if (close_exp_file) fclose (exp_file);
  if (close_aig_file) fclose (aig_file);
  if (close_cnf_file) fclose (cnf_file);
  if (err) return BTOR_ERR_EXIT;
  if (done) return BTOR_SUCC_EXIT;
  if (sat_result == BTOR_UNSAT) return BTOR_UNSAT_EXIT;
  assert (sat_result == BTOR_SAT);
  return BTOR_SAT_EXIT;
}
