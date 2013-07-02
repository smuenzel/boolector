#include "btoribv.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
};

#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace std;

struct Var
{
  string name;
  unsigned width;
  Var () {}
  Var (string n, int w) : name (n), width (w) {}
};

static BtorIBV* ibvm;

static map<string, unsigned> symtab;
static map<unsigned, Var> idtab;
static vector<unsigned> vartab;

static int lineno             = 1;
static FILE* input            = stdin;
static const char* input_name = "<stdin>";
static bool close_input;

static char *line, *nts;
static int szline, nline;

static struct
{
  int addAssertion;
  int addAssignment;
  int addAssumption;
  int addBitAnd;
  int addBitNot;
  int addBitOr;
  int addBitXor;
  int addCase;
  int addConcat;
  int addCondition;
  int addConstant;
  int addDiv;
  int addEqual;
  int addLessThan;
  int addLessEqual;
  int addGreaterThan;
  int addGreaterEqual;
  int addLogicalAnd;
  int addLogicalNot;
  int addLogicalOr;
  int addLShift;
  int addMod;
  int addMul;
  int addNonState;
  int addRangeName;
  int addReplicate;
  int addRShift;
  int addSignExtension;
  int addState;
  int addSub;
  int addSum;
  int addVariable;
  int addZeroExtension;

} stats;

static void
err (const char* fmt, ...)
{
  va_list ap;
  fputs ("*** btorimc: ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  fflush (stderr);
  exit (1);
}

static void
msg (const char* fmt, ...)
{
  va_list ap;
  fputs ("[btorimc] ", stdout);
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  fputc ('\n', stdout);
  fflush (stdout);
}

static void
perr (const char* fmt, ...)
{
  va_list ap;
  fprintf (stderr, "%s:%d: ", input_name, lineno);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  fflush (stderr);
  exit (1);
}

static void
pushch (int ch)
{
  if (nline >= szline)
  {
    szline = szline ? 2 * szline : 1;
    line   = (char*) realloc (line, szline);
  }
  line[nline++] = ch;
}

static bool
read_line ()
{
  nline = 0;
  int ch;
  while ((ch = getc (input)) != '\n')
  {
    if (ch == ' ' || ch == '\t' || ch == '\r') continue;
    if (ch == EOF)
    {
      if (nline) perr ("unexpected end-of-file");
      if (line) free (line);
#if 0  // TODO print stats
      if (stats.vars) msg ("parsed %d variables", stats.vars);
      if (stats.rangenames) msg ("parsed %d range names", stats.rangenames);
#endif
      return false;
    }
    pushch (ch);
  }
  pushch (0);
  return true;
}

static int
str_to_id_or_number (const char* s)
{
  return atoi ((s[0] == 'i' && s[1] == 'd' && s[2] == '=') ? s + 3 : s);
}

#define CHKARGS(EXPECTED)                                             \
  do                                                                  \
  {                                                                   \
    if (EXPECTED != size - 1)                                         \
      perr ("operator '%s' expected exactly %d arguments but got %d", \
            op,                                                       \
            EXPECTED,                                                 \
            size - 1);                                                \
  } while (0)

#define CHKIDUNUSED(ID)                                                   \
  do                                                                      \
  {                                                                       \
    if (idtab.find (ID) != idtab.end ()) perr ("id %u already used", ID); \
  } while (0)

#define CHKID(ID)                                                      \
  do                                                                   \
  {                                                                    \
    if (idtab.find (ID) == idtab.end ()) perr ("id %u undefined", ID); \
  } while (0)

#define CHKSYMUNUSED(SYM)                              \
  do                                                   \
  {                                                    \
    if (symtab.find (SYM) != symtab.end ())            \
      perr ("symbol '%s' already used", SYM.c_str ()); \
  } while (0)

#define N(I) \
  (assert ((I) < size), (unsigned) str_to_id_or_number (toks[I].c_str ()))

#define T(I) (assert ((I) < size), toks[I])

#define CHKBIT(SYM, B)                                                         \
  do                                                                           \
  {                                                                            \
    if (symtab.find (SYM) == symtab.end ())                                    \
      perr ("symbol '%s' undefined", (SYM).c_str ());                          \
    Var& V = idtab[symtab[(SYM)]];                                             \
    if ((B) >= V.width) perr ("BIT %u >= width of '%s'", (B), (SYM).c_str ()); \
  } while (0)

#define BIT(NAME, SYM, B) \
  CHKBIT (SYM, B);        \
  BitVector::Bit NAME (symtab[(SYM)], B)

#define CHKRANGE(SYM, MSB, LSB)                                \
  do                                                           \
  {                                                            \
    if (symtab.find (SYM) == symtab.end ())                    \
      perr ("symbol '%s' undefined", (SYM).c_str ());          \
    if ((MSB) < (LSB)) perr ("MSB %u < LSB %u", (MSB), (LSB)); \
    Var& V = idtab[symtab[(SYM)]];                             \
    if ((MSB) >= V.width)                                      \
      perr ("MSB %u >= width of '%s'", (MSB), (SYM).c_str ()); \
  } while (0)

#define RANGE(NAME, SYM, MSB, LSB) \
  CHKRANGE (SYM, MSB, LSB);        \
  BitVector::BitRange NAME (symtab[(SYM)], MSB, LSB)

#define CHKRANGESAMEWIDTH(RANGE0, RANGE1)             \
  do                                                  \
  {                                                   \
    if (RANGE0.getWidth () != RANGE1.getWidth ())     \
      perr ("range [%u:%u] and [%u:%u] do not match", \
            RANGE0.m_nMsb,                            \
            RANGE0.m_nLsb,                            \
            RANGE1.m_nMsb,                            \
            RANGE1.m_nLsb);                           \
  } while (0)

#define UNARY(NAME)                 \
  (!strcmp (op, #NAME)) do          \
  {                                 \
    CHKARGS (6);                    \
    RANGE (c, T (1), N (2), N (3)); \
    RANGE (n, T (4), N (5), N (6)); \
    CHKRANGESAMEWIDTH (c, n);       \
    ibvm->NAME (c, n);              \
    stats.NAME++;                   \
  }                                 \
  while (0)

#define EXTEND(NAME)                                     \
  (!strcmp (op, #NAME)) do                               \
  {                                                      \
    CHKARGS (6);                                         \
    RANGE (c, T (1), N (2), N (3));                      \
    RANGE (n, T (4), N (5), N (6));                      \
    if (c.getWidth () < n.getWidth ())                   \
      perr ("range [%u:%u] smaller than range [%u:%u] ", \
            c.m_nMsb,                                    \
            c.m_nLsb,                                    \
            n.m_nMsb,                                    \
            n.m_nLsb);                                   \
    ibvm->NAME (c, n);                                   \
    stats.NAME++;                                        \
  }                                                      \
  while (0)

#define BINARY(NAME)                \
  (!strcmp (op, #NAME)) do          \
  {                                 \
    CHKARGS (9);                    \
    RANGE (c, T (1), N (2), N (3)); \
    RANGE (a, T (4), N (5), N (6)); \
    RANGE (b, T (7), N (8), N (9)); \
    CHKRANGESAMEWIDTH (c, a);       \
    CHKRANGESAMEWIDTH (c, b);       \
    ibvm->NAME (c, a, b);           \
    stats.NAME++;                   \
  }                                 \
  while (0)

#define UNARYARG(NAME)              \
  (!strcmp (op, #NAME)) do          \
  {                                 \
    CHKARGS (7);                    \
    RANGE (c, T (1), N (2), N (3)); \
    RANGE (n, T (4), N (5), N (6)); \
    CHKRANGESAMEWIDTH (c, n);       \
    unsigned arg = N (7);           \
    ibvm->NAME (c, n, arg);         \
    stats.NAME++;                   \
  }                                 \
  while (0)

#define PRED1(NAME)                 \
  (!strcmp (op, #NAME)) do          \
  {                                 \
    CHKARGS (5);                    \
    RANGE (c, T (1), N (2), N (2)); \
    RANGE (a, T (3), N (4), N (5)); \
    assert (c.getWidth () == 1);    \
    ibvm->NAME (c, a);              \
    stats.NAME++;                   \
  }                                 \
  while (0)

#define PRED2(NAME)                 \
  (!strcmp (op, #NAME)) do          \
  {                                 \
    CHKARGS (8);                    \
    RANGE (c, T (1), N (2), N (2)); \
    RANGE (a, T (3), N (4), N (5)); \
    RANGE (b, T (6), N (7), N (8)); \
    assert (c.getWidth () == 1);    \
    CHKRANGESAMEWIDTH (a, b);       \
    ibvm->NAME (c, a, b);           \
    stats.NAME++;                   \
  }                                 \
  while (0)

static const char*
firstok ()
{
  for (nts = line; *nts && *nts != '('; nts++)
    ;
  if (!*nts) return 0;
  assert (*nts == '(');
  *nts++ = 0;
  return line;
}

static const char*
nextok ()
{
  const char* res;
  int open;
  if (nts >= line + nline) return 0;
  while (isspace (*nts)) nts++;
  if (!*nts) return 0;
  res  = nts;
  open = 0;
  for (;;)
  {
    int ch = *nts;
    if (!ch)
      perr ("unexpected end-of-line");
    else if (ch == '\\' && !*++nts)
      perr ("unexpected end-of-line after '\\'");
    else if (ch == '(')
      open++, assert (open > 0);
    else if (ch == ',' && !open)
      break;
    else if (ch == ')')
    {
      if (open > 0)
        open--;
      else
      {
        assert (!open);
        if (nts[1]) perr ("trailing characters after last ')'");
        break;
      }
    }
    nts++;
  }
  *nts++  = 0;
  char* p = nts - 2;
  while (p >= res && isspace (*p)) *p-- = 0;
  return *res ? res : 0;
}

static void
parse_line ()
{
  const char* str;
  vector<string> toks;
  char* p;
  for (p = line; *p; p++)
    ;
  if (p == line) perr ("empty line");
  if (p[-1] != ')') perr ("line does not end with ')'");
  if (!(str = firstok ())) perr ("'(' missing");
  toks.push_back (string (str));
  while ((str = nextok ())) toks.push_back (string (str));
#if 1
  printf ("[btorimc] line %d:", lineno);
  for (vector<string>::const_iterator it = toks.begin (); it != toks.end ();
       it++)
  {
    printf (" %s", (*it).c_str ());
  }
  printf ("\n");
#endif
  size_t size = toks.size ();
  assert (size > 0);
  const char* op = toks[0].c_str ();
  if (!strcmp (op, "addVariable"))
  {
    if (size < 7)
      perr ("operator 'addVariable' expected 6 or 7 arguments but only got %d",
            size - 1);
    else if (size > 8)
      perr ("operator 'addVariable' expected only 6 or 7 arguments but got %d",
            size - 1);
    assert (size == 7 || size == 8);
    string sym  = T (2);
    unsigned id = N (1);
    CHKIDUNUSED (id);
    CHKSYMUNUSED (sym);
    unsigned width = N (3);
    if (width <= 0) perr ("expected positive width but got %u", width);
    symtab[sym] = id;
    Var v (sym, width);
    idtab[id] = Var (sym, width);
    stats.addVariable++;
    if (size == 8)
      ibvm->addVariableOld (id,
                            sym,
                            width,
                            (bool) N (4),
                            (bool) N (5),
                            (bool) N (6),
                            (BitVector::DirectionKind) N (7));
    else
      ibvm->addVariable (id,
                         sym,
                         width,
                         (bool) N (4),
                         (BitVector::BvVariableSource) N (5),
                         (BitVector::DirectionKind) N (6));
    vartab.push_back (id);
  }
  else if (!strcmp (op, "addRangeName"))
  {
    CHKARGS (6);
    RANGE (range, T (1), N (2), N (3));
    unsigned msb = N (5), lsb = N (6);
    if (msb < lsb) perr ("MSB %u < LSB %u", msb, lsb);
    ibvm->addRangeName (range, T (4), msb, lsb);
    stats.addRangeName++;
  }
  else if (!strcmp (op, "addState"))
  {
    CHKARGS (9);
    RANGE (n, T (1), N (2), N (3));
    bool undeclared = (T (4) == "undeclared");
    RANGE (next, T (7), N (8), N (9));
    CHKRANGESAMEWIDTH (n, next);
    if (!undeclared) CHKRANGE (T (4), N (5), N (6));
    BitVector::BitRange init (undeclared ? 0 : symtab[T (4)],
                              undeclared ? 0 : N (5),
                              undeclared ? 0 : N (6));
    if (!undeclared) CHKRANGESAMEWIDTH (n, init);
    ibvm->addState (n, init, next);
    stats.addState++;
  }
  else if (!strcmp (op, "addConstant"))
  {
    CHKARGS (3);
    unsigned id = N (1);
    CHKIDUNUSED (id);
    unsigned width = N (3);
    if (T (2).size () != width)
      perr ("constant string '%s' does not match width %u",
            T (2).c_str (),
            width);
    idtab[id] = Var (T (2), width);
    {
      // TODO: hack to get 'LSD' example through ...
      char buf[20];
      sprintf (buf, "%u", id);
      string sym (buf);
      symtab["b0_v" + sym] = id;
      symtab["b1_v" + sym] = id;
    }
    {
      // TODO: hack to get 'regtoy' examples through ...
      char buf[20];
      sprintf (buf, "%u", id);
      string sym  = string ("const(") + T (2) + ")";
      symtab[sym] = id;
    }
    symtab[T (2)] = id;
    ibvm->addConstant (id, T (2), width);
    stats.addConstant++;
  }
  else if (!strcmp (op, "addCondition"))
  {
    CHKARGS (12);
    RANGE (n, T (1), N (2), N (3));
    RANGE (c, T (4), N (5), N (6));
    RANGE (t, T (7), N (8), N (9));
    RANGE (e, T (10), N (11), N (12));
    CHKRANGESAMEWIDTH (n, t);
    CHKRANGESAMEWIDTH (n, e);
    if (c.getWidth () != 1) CHKRANGESAMEWIDTH (n, c);
    ibvm->addCondition (n, c, t, e);
    stats.addCondition++;
  }
  else if (!strcmp (op, "addCase"))
  {
  ADDCASE:
    if (size < 5) perr ("non enough arguments for 'addCase'");
    RANGE (n, T (1), N (2), N (3));
    unsigned nargs = N (4);
    if (!nargs) perr ("no cases");
    if (nargs & 1) perr ("odd number of 'addCase' arguments %u", nargs);
    if (size != 3 * nargs + 5)
      perr ("number of 'addCase' arguments does not match");
    vector<BitVector::BitRange> args;
    for (unsigned i = 5; nargs; i += 3, nargs--)
    {
      bool undeclared = (T (i) == "undeclared");
      if (undeclared && (nargs & 1)) perr ("'undeclared' at wrong position");
      if (!undeclared) CHKRANGE (T (i), N (i + 1), N (i + 2));
      BitVector::BitRange arg (undeclared ? 0 : symtab[T (i)],
                               undeclared ? 0 : N (i + 1),
                               undeclared ? 0 : N (i + 2));
      if (!undeclared && !(nargs & 1) && arg.getWidth () != 1)
        CHKRANGESAMEWIDTH (n, arg);
      args.push_back (arg);
    }
    ibvm->addCase (n, args);
    stats.addCase++;
  }
  else if (!strcmp (op, "addParallelCase"))
    goto ADDCASE;
  else if (!strcmp (op, "addConcat"))
  {
    if (size < 5) perr ("not enough arguments for 'addConcat'");
    RANGE (n, T (1), N (2), N (3));
    unsigned nargs = N (4);
    if (!nargs) perr ("no arguments");
    if (size != 3 * nargs + 5)
      perr ("number of 'addConcat' arguments does not match");
    vector<BitVector::BitRange> args;
    unsigned width = 0;
    for (unsigned i = 5; nargs; i += 3, nargs--)
    {
      CHKRANGE (T (i), N (i + 1), N (i + 2));
      BitVector::BitRange arg (symtab[T (i)], N (i + 1), N (i + 2));
      args.push_back (arg);
      width += arg.getWidth ();
    }
    if (width != n.getWidth ())
      perr ("sum of width of 'addConcat' arguments does not match %u",
            n.getWidth ());
    ibvm->addConcat (n, args);
    stats.addConcat++;
  }
  else if
    UNARY (addNonState);
  else if
    UNARY (addAssignment);
  else if
    UNARY (addBitNot);
  else if
    EXTEND (addZeroExtension);
  else if
    EXTEND (addSignExtension);
  else if
    PRED1 (addLogicalNot);
  else if
    UNARYARG (addRShift);
  else if
    UNARYARG (addLShift);
  else if
    BINARY (addState);
  else if
    BINARY (addBitAnd);
  else if
    BINARY (addBitOr);
  else if
    BINARY (addBitXor);
  else if
    BINARY (addDiv);
  else if
    BINARY (addMod);
  else if
    BINARY (addMul);
  else if
    BINARY (addSum);
  else if
    BINARY (addSub);
  else if
    PRED2 (addEqual);
  else if
    PRED2 (addLogicalOr);
  else if
    PRED2 (addLogicalAnd);
  else if
    PRED2 (addLessThan);
  else if
    PRED2 (addLessEqual);
  else if
    PRED2 (addGreaterEqual);
  else if
    PRED2 (addGreaterThan);
  else if (!strcmp (op, "addReplicate"))
  {
    CHKARGS (7);
    RANGE (c, T (1), N (2), N (3));
    RANGE (n, T (4), N (5), N (6));
    if (c.getWidth () != N (7) * (long long) n.getWidth ())
      perr ("range [%u:%u] does not match %u times range [%u:%u] ",
            c.m_nMsb,
            c.m_nLsb,
            N (7),
            n.m_nMsb,
            n.m_nLsb);
    ibvm->addReplicate (c, n, N (7));
    stats.addReplicate++;
  }
  else if (!strcmp (op, "addAssertion"))
  {
    CHKARGS (2);
    RANGE (r, T (1), N (2), N (2));
    if (r.getWidth () != 1) perr ("invalid assertion width %u", r.getWidth ());
    ibvm->addAssertion (r);
    stats.addAssertion++;
  }
  else if (!strcmp (op, "addAssumption"))
  {
    CHKARGS (3);
    RANGE (r, T (1), N (2), N (2));
    if (r.getWidth () != 1) perr ("invalid assumption width %u", r.getWidth ());
    ibvm->addAssumption (r, (bool) N (3));
    stats.addAssumption++;
  }
  else
    perr ("unknown operator '%s'", op);
}

static void
parse ()
{
  while (read_line ()) parse_line (), lineno++;
}

static int
hasuffix (const char* arg, const char* suffix)
{
  if (strlen (arg) < strlen (suffix)) return 0;
  if (strcmp (arg + strlen (arg) - strlen (suffix), suffix)) return 0;
  return 1;
}

static bool
cmd (const char* arg, const char* suffix, const char* fmt)
{
  struct stat buf;
  char* cmd;
  int len;
  if (!hasuffix (arg, suffix)) return 0;
  if (stat (arg, &buf)) err ("can not stat file '%s'", arg);
  len = strlen (fmt) + strlen (arg) + 1;
  cmd = (char*) malloc (len);
  sprintf (cmd, fmt, arg);
  input = popen (cmd, "r");
  free (cmd);
  close_input = 2;
  return 1;
}

static bool
isfile (const char* p)
{
  struct stat buf;
  return !stat (p, &buf);
}

static bool
isbound (const char* str)
{
  const char* p = str;
  if (!isdigit (*p++)) return 0;
  while (isdigit (*p)) p++;
  return !*p;
}

static const char* USAGE =
    "usage: btorimc [ <option> ... ] [<k>] [<ibv>]\n"
    "\n"
    "where <option> is one of the following:\n"
    "\n"
    "  -h    print this command line option summary\n"
    "  -n    do not print witness trace\n"
    "  -d    dump BTOR model to stdout\n"
    "\n"
    "and\n"
    "\n"
    "<k>     maximal bound for bounded model checking (default 10)\n"
    "<ibv>   IBV input file (default '<stdin>')\n";

int
main (int argc, char** argv)
{
  bool witness = true, dump = false;
  ;
  int k = -1, r;
  for (int i = 1; i < argc; i++)
  {
    if (!strcmp (argv[i], "-h"))
    {
      fputs (USAGE, stdout);
      exit (0);
    }
    else if (!strcmp (argv[i], "-n"))
      witness = false;
    else if (!strcmp (argv[i], "-d"))
      dump = true;
    else if (argv[i][0] == '-')
      err ("invalid command line option '%s'", argv[i]);
    else if (isbound (argv[i]))
    {
      if (k >= 0) err ("more than one bound");
      if ((k = atoi (argv[i])) < 0) err ("invalid bound");
    }
    else if (close_input)
      err ("more than one input");
    else if (!isfile (argv[i]))
      err ("'%s' does not seem to be a file", argv[i]);
    else if (cmd ((input_name = argv[i]), ".gz", "gunzip -c %s"))
      ;
    else if (cmd (argv[i], ".bz2", "bzcat %s"))
      ;
    else if (!(input = fopen (argv[i], "r")))
      err ("can not read '%s'", argv[i]);
    else
      close_input = true;
  }
  msg ("reading '%s'", input_name);
  ibvm = new BtorIBV ();
  ibvm->setVerbosity (10);
  if (witness) ibvm->enableTraceGeneration ();
  parse ();
  if (close_input == 1) fclose (input);
  if (close_input == 2) pclose (input);
  ibvm->analyze ();
  ibvm->translate ();
  if (dump)
  {
    msg ("dumping BTOR model to '<stdout>'");
    ibvm->dump_btor (stdout);
  }
  else
  {
    if (k < 0) k = 10;
    msg ("running bounded model checking up to bound %d", k);
    r = ibvm->bmc (k);
    if (r < 0)
      msg ("property not reachable until bound %d", k);
    else
    {
      msg ("property reachable at bound %d", r);
      if (witness)
      {
        for (int i = 0; i <= r; i++)
        {
          msg ("time frame %d", i);
          for (vector<unsigned>::const_iterator it = vartab.begin ();
               it != vartab.end ();
               it++)
          {
            unsigned id = *it;
            printf ("time=%d id=%d ", i, id);
            Var& var = idtab[id];
            printf ("%s ", var.name.c_str ());
            assert (var.width > 0);
            BitVector::BitRange range (id, var.width - 1, 0);
            string val = ibvm->assignment (range, i);
            printf ("%s\n", val.c_str ());
          }
        }
      }
      else
        msg ("disabled witness printing with '-n'");
    }
  }
  delete ibvm;
  return 0;
}
