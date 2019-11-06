/* Output the generated parsing program for Bison.

   Copyright (C) 1984, 1986, 1989, 1992, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008 Free Software Foundation, Inc.

   This file is part of Bison, the GNU Compiler Compiler.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include "system.h"

#include <configmake.h>
#include <error.h>
#include <get-errno.h>
#include <quotearg.h>
#include <subpipe.h>
#include <timevar.h>

#include "complain.h"
#include "files.h"
#include "getargs.h"
#include "gram.h"
#include "muscle_tab.h"
#include "output.h"
#include "reader.h"
#include "scan-code.h"    /* max_left_semantic_context */
#include "scan-skel.h"
#include "symtab.h"
#include "tables.h"
#include <relocatable.h>


static struct obstack format_obstack;


/*-------------------------------------------------------------------.
| Create a function NAME which associates to the muscle NAME the     |
| result of formatting the FIRST and then TABLE_DATA[BEGIN..END[ (of |
| TYPE), and to the muscle NAME_max, the max value of the            |
| TABLE_DATA.                                                        |
`-------------------------------------------------------------------*/


#define GENERATE_MUSCLE_INSERT_TABLE(Name, Type)			\
									\
static void								\
Name (char const *name,							\
      Type *table_data,							\
      Type first,							\
      int begin,							\
      int end)								\
{									\
  Type min = first;							\
  Type max = first;							\
  long int lmin;							\
  long int lmax;							\
  int i;								\
  int j = 1;								\
									\
  obstack_fgrow1 (&format_obstack, "%6d", first);			\
  for (i = begin; i < end; ++i)						\
    {									\
      obstack_1grow (&format_obstack, ',');				\
      if (j >= 10)							\
	{								\
	  obstack_sgrow (&format_obstack, "\n  ");			\
	  j = 1;							\
	}								\
      else								\
	++j;								\
      obstack_fgrow1 (&format_obstack, "%6d", table_data[i]);		\
      if (table_data[i] < min)						\
	min = table_data[i];						\
      if (max < table_data[i])						\
	max = table_data[i];						\
    }									\
  obstack_1grow (&format_obstack, 0);					\
  muscle_insert (name, obstack_finish (&format_obstack));		\
									\
  lmin = min;								\
  lmax = max;								\
  /* Build `NAME_min' and `NAME_max' in the obstack. */			\
  obstack_fgrow1 (&format_obstack, "%s_min", name);			\
  obstack_1grow (&format_obstack, 0);					\
  MUSCLE_INSERT_LONG_INT (obstack_finish (&format_obstack), lmin);	\
  obstack_fgrow1 (&format_obstack, "%s_max", name);			\
  obstack_1grow (&format_obstack, 0);					\
  MUSCLE_INSERT_LONG_INT (obstack_finish (&format_obstack), lmax);	\
}

GENERATE_MUSCLE_INSERT_TABLE(muscle_insert_unsigned_int_table, unsigned int)
GENERATE_MUSCLE_INSERT_TABLE(muscle_insert_int_table, int)
GENERATE_MUSCLE_INSERT_TABLE(muscle_insert_base_table, base_number)
GENERATE_MUSCLE_INSERT_TABLE(muscle_insert_rule_number_table, rule_number)
GENERATE_MUSCLE_INSERT_TABLE(muscle_insert_symbol_number_table, symbol_number)
GENERATE_MUSCLE_INSERT_TABLE(muscle_insert_item_number_table, item_number)
GENERATE_MUSCLE_INSERT_TABLE(muscle_insert_state_number_table, state_number)


/*--------------------------------------------------------------------.
| Print to OUT a representation of STRING escaped both for C and M4.  |
`--------------------------------------------------------------------*/

static void
escaped_output (FILE *out, char const *string)
{
  char const *p;
  fprintf (out, "[[");

  for (p = quotearg_style (c_quoting_style, string); *p; p++)
    switch (*p)
      {
      case '$': fputs ("$][", out); break;
      case '@': fputs ("@@",  out); break;
      case '[': fputs ("@{",  out); break;
      case ']': fputs ("@}",  out); break;
      default: fputc (*p, out); break;
      }

  fprintf (out, "]]");
}


/*------------------------------------------------------------------.
| Prepare the muscles related to the symbols: translate, tname, and |
| toknum.                                                           |
`------------------------------------------------------------------*/

static void
prepare_symbols (void)
{
  MUSCLE_INSERT_BOOL ("token_table", token_table_flag);
  MUSCLE_INSERT_INT ("tokens_number", ntokens);
  MUSCLE_INSERT_INT ("nterms_number", nvars);
  MUSCLE_INSERT_INT ("undef_token_number", undeftoken->number);
  MUSCLE_INSERT_INT ("user_token_number_max", max_user_token_number);

  muscle_insert_symbol_number_table ("translate",
				     token_translations,
				     token_translations[0],
				     1, max_user_token_number + 1);

  /* tname -- token names.  */
  {
    int i;
    /* We assume that the table will be output starting at column 2. */
    int j = 2;
    struct quoting_options *qo = clone_quoting_options (0);
    set_quoting_style (qo, c_quoting_style);
    set_quoting_flags (qo, QA_SPLIT_TRIGRAPHS);
    for (i = 0; i < nsyms; i++)
      {
	char *cp = quotearg_alloc (symbols[i]->tag, -1, qo);
	/* Width of the next token, including the two quotes, the
	   comma and the space.  */
	int width = strlen (cp) + 2;

	if (j + width > 75)
	  {
	    obstack_sgrow (&format_obstack, "\n ");
	    j = 1;
	  }

	if (i)
	  obstack_1grow (&format_obstack, ' ');
	MUSCLE_OBSTACK_SGROW (&format_obstack, cp);
        free (cp);
	obstack_1grow (&format_obstack, ',');
	j += width;
      }
    free (qo);
    obstack_sgrow (&format_obstack, " ]b4_null[");

    /* Finish table and store. */
    obstack_1grow (&format_obstack, 0);
    muscle_insert ("tname", obstack_finish (&format_obstack));
  }

  /* Output YYTOKNUM. */
  {
    int i;
    int *values = xnmalloc (ntokens, sizeof *values);
    for (i = 0; i < ntokens; ++i)
      values[i] = symbols[i]->user_token_number;
    muscle_insert_int_table ("toknum", values,
			     values[0], 1, ntokens);
    free (values);
  }
}


/*-------------------------------------------------------------.
| Prepare the muscles related to the rules: rhs, prhs, r1, r2, |
| rline, dprec, merger.                                        |
`-------------------------------------------------------------*/

static void
prepare_rules (void)
{
  rule_number r;
  unsigned int i = 0;
  item_number *rhs = xnmalloc (nritems, sizeof *rhs);
  unsigned int *prhs = xnmalloc (nrules, sizeof *prhs);
  unsigned int *rline = xnmalloc (nrules, sizeof *rline);
  symbol_number *r1 = xnmalloc (nrules, sizeof *r1);
  unsigned int *r2 = xnmalloc (nrules, sizeof *r2);
  int *dprec = xnmalloc (nrules, sizeof *dprec);
  int *merger = xnmalloc (nrules, sizeof *merger);

  for (r = 0; r < nrules; ++r)
    {
      item_number *rhsp = NULL;
      /* Index of rule R in RHS. */
      prhs[r] = i;
      /* RHS of the rule R. */
      for (rhsp = rules[r].rhs; *rhsp >= 0; ++rhsp)
	rhs[i++] = *rhsp;
      /* LHS of the rule R. */
      r1[r] = rules[r].lhs->number;
      /* Length of rule R's RHS. */
      r2[r] = i - prhs[r];
      /* Separator in RHS. */
      rhs[i++] = -1;
      /* Line where rule was defined. */
      rline[r] = rules[r].location.start.line;
      /* Dynamic precedence (GLR).  */
      dprec[r] = rules[r].dprec;
      /* Merger-function index (GLR).  */
      merger[r] = rules[r].merger;
    }
  aver (i == nritems);

  muscle_insert_item_number_table ("rhs", rhs, ritem[0], 1, nritems);
  muscle_insert_unsigned_int_table ("prhs", prhs, 0, 0, nrules);
  muscle_insert_unsigned_int_table ("rline", rline, 0, 0, nrules);
  muscle_insert_symbol_number_table ("r1", r1, 0, 0, nrules);
  muscle_insert_unsigned_int_table ("r2", r2, 0, 0, nrules);
  muscle_insert_int_table ("dprec", dprec, 0, 0, nrules);
  muscle_insert_int_table ("merger", merger, 0, 0, nrules);

  MUSCLE_INSERT_INT ("rules_number", nrules);
  MUSCLE_INSERT_INT ("max_left_semantic_context", max_left_semantic_context);

  free (rhs);
  free (prhs);
  free (rline);
  free (r1);
  free (r2);
  free (dprec);
  free (merger);
}

/*--------------------------------------------.
| Prepare the muscles related to the states.  |
`--------------------------------------------*/

static void
prepare_states (void)
{
  state_number i;
  symbol_number *values = xnmalloc (nstates, sizeof *values);
  for (i = 0; i < nstates; ++i)
    values[i] = states[i]->accessing_symbol;
  muscle_insert_symbol_number_table ("stos", values,
				     0, 1, nstates);
  free (values);

  MUSCLE_INSERT_INT ("last", high);
  MUSCLE_INSERT_INT ("final_state_number", final_state->number);
  MUSCLE_INSERT_INT ("states_number", nstates);
}



/*---------------------------------.
| Output the user actions to OUT.  |
`---------------------------------*/

static void
user_actions_output (FILE *out)
{
  rule_number r;

  fputs ("m4_define([b4_actions], \n[", out);
  for (r = 0; r < nrules; ++r)
    if (rules[r].action)
      {
	fprintf (out, "b4_case(%d, [b4_syncline(%d, ", r + 1,
		 rules[r].action_location.start.line);
	escaped_output (out, rules[r].action_location.start.file);
	fprintf (out, ")\n[    %s]])\n\n", rules[r].action);
      }
  fputs ("])\n\n", out);
}

/*--------------------------------------.
| Output the merge functions to OUT.   |
`--------------------------------------*/

static void
merger_output (FILE *out)
{
  int n;
  merger_list* p;

  fputs ("m4_define([b4_mergers], \n[[", out);
  for (n = 1, p = merge_functions; p != NULL; n += 1, p = p->next)
    {
      if (p->type[0] == '\0')
	fprintf (out, "  case %d: *yy0 = %s (*yy0, *yy1); break;\n",
		 n, p->name);
      else
	fprintf (out, "  case %d: yy0->%s = %s (*yy0, *yy1); break;\n",
		 n, p->type, p->name);
    }
  fputs ("]])\n\n", out);
}

/*--------------------------------------.
| Output the tokens definition to OUT.  |
`--------------------------------------*/

static void
token_definitions_output (FILE *out)
{
  int i;
  char const *sep = "";

  fputs ("m4_define([b4_tokens], \n[", out);
  for (i = 0; i < ntokens; ++i)
    {
      symbol *sym = symbols[i];
      int number = sym->user_token_number;

      /* At this stage, if there are literal aliases, they are part of
	 SYMBOLS, so we should not find symbols which are the aliases
	 here.  */
      aver (number != USER_NUMBER_ALIAS);

      /* Skip error token.  */
      if (sym == errtoken)
	continue;

      /* If this string has an alias, then it is necessarily the alias
	 which is to be output.  */
      if (sym->alias)
	sym = sym->alias;

      /* Don't output literal chars or strings (when defined only as a
	 string).  Note that must be done after the alias resolution:
	 think about `%token 'f' "f"'.  */
      if (sym->tag[0] == '\'' || sym->tag[0] == '\"')
	continue;

      /* Don't #define nonliteral tokens whose names contain periods
	 or '$' (as does the default value of the EOF token).  */
      if (strchr (sym->tag, '.') || strchr (sym->tag, '$'))
	continue;

      fprintf (out, "%s[[[%s]], %d]",
	       sep, sym->tag, number);
      sep = ",\n";
    }
  fputs ("])\n\n", out);
}


/*---------------------------------------------------.
| Output the symbol destructors or printers to OUT.  |
`---------------------------------------------------*/

static void
symbol_code_props_output (FILE *out, char const *what,
                          code_props const *(*get)(symbol const *))
{
  int i;
  char const *sep = "";

  fputs ("m4_define([b4_symbol_", out);
  fputs (what, out);
  fputs ("], \n[", out);
  for (i = 0; i < nsyms; ++i)
    {
      symbol *sym = symbols[i];
      char const *code = (*get) (sym)->code;
      if (code)
        {
          location loc = (*get) (sym)->location;
          /* Filename, lineno,
             Symbol-name, Symbol-number,
             code, optional typename.  */
          fprintf (out, "%s[", sep);
          sep = ",\n";
          escaped_output (out, loc.start.file);
          fprintf (out, ", %d, ", loc.start.line);
          escaped_output (out, sym->tag);
          fprintf (out, ", %d, [[%s]]", sym->number, code);
          if (sym->type_name)
            fprintf (out, ", [[%s]]", sym->type_name);
          fputc (']', out);
        }
    }
  fputs ("])\n\n", out);
}


static void
prepare_actions (void)
{
  /* Figure out the actions for the specified state, indexed by
     lookahead token type.  */

  muscle_insert_rule_number_table ("defact", yydefact,
				   yydefact[0], 1, nstates);

  /* Figure out what to do after reducing with each rule, depending on
     the saved state from before the beginning of parsing the data
     that matched this rule.  */
  muscle_insert_state_number_table ("defgoto", yydefgoto,
				    yydefgoto[0], 1, nsyms - ntokens);


  /* Output PACT. */
  muscle_insert_base_table ("pact", base,
			     base[0], 1, nstates);
  MUSCLE_INSERT_INT ("pact_ninf", base_ninf);

  /* Output PGOTO. */
  muscle_insert_base_table ("pgoto", base,
			     base[nstates], nstates + 1, nvectors);

  muscle_insert_base_table ("table", table,
			    table[0], 1, high + 1);
  MUSCLE_INSERT_INT ("table_ninf", table_ninf);

  muscle_insert_base_table ("check", check,
			    check[0], 1, high + 1);

  /* GLR parsing slightly modifies YYTABLE and YYCHECK (and thus
     YYPACT) so that in states with unresolved conflicts, the default
     reduction is not used in the conflicted entries, so that there is
     a place to put a conflict pointer.

     This means that YYCONFLP and YYCONFL are nonsense for a non-GLR
     parser, so we could avoid accidents by not writing them out in
     that case.  Nevertheless, it seems even better to be able to use
     the GLR skeletons even without the non-deterministic tables.  */
  muscle_insert_unsigned_int_table ("conflict_list_heads", conflict_table,
				    conflict_table[0], 1, high + 1);
  muscle_insert_unsigned_int_table ("conflicting_rules", conflict_list,
				    0, 1, conflict_list_cnt);
}


/*---------------------------.
| Call the skeleton parser.  |
`---------------------------*/

static void
output_skeleton (void)
{
  FILE *in;
  FILE *out;
  int filter_fd[2];
  char const *argv[9];
  pid_t pid;

  /* Compute the names of the package data dir and skeleton files.  */
  char const m4sugar[] = "m4sugar/m4sugar.m4";
  char const m4bison[] = "bison.m4";
  char *full_m4sugar;
  char *full_m4bison;
  char *full_skeleton;
  char const *p;
  char const *m4 = (p = getenv ("M4")) ? p : M4;
  char const *pkgdatadir = compute_pkgdatadir ();
  size_t skeleton_size = strlen (skeleton) + 1;
  size_t pkgdatadirlen = strlen (pkgdatadir);
  while (pkgdatadirlen && pkgdatadir[pkgdatadirlen - 1] == '/')
    pkgdatadirlen--;
  full_skeleton = xmalloc (pkgdatadirlen + 1
			   + (skeleton_size < sizeof m4sugar
			      ? sizeof m4sugar : skeleton_size));
  strncpy (full_skeleton, pkgdatadir, pkgdatadirlen);
  full_skeleton[pkgdatadirlen] = '/';
  strcpy (full_skeleton + pkgdatadirlen + 1, m4sugar);
  full_m4sugar = xstrdup (full_skeleton);
  strcpy (full_skeleton + pkgdatadirlen + 1, m4bison);
  full_m4bison = xstrdup (full_skeleton);
  if (strchr (skeleton, '/'))
    strcpy (full_skeleton, skeleton);
  else
    strcpy (full_skeleton + pkgdatadirlen + 1, skeleton);

  /* Test whether m4sugar.m4 is readable, to check for proper
     installation.  A faulty installation can cause deadlock, so a
     cheap sanity check is worthwhile.  */
  xfclose (xfopen (full_m4sugar, "r"));

  /* Create an m4 subprocess connected to us via two pipes.  */

  if (trace_flag & trace_tools)
    fprintf (stderr, "running: %s %s - %s %s\n",
             m4, full_m4sugar, full_m4bison, full_skeleton);

  /* Some future version of GNU M4 (most likely 1.6) may treat the -dV in a
     position-dependent manner.  Keep it as the first argument so that all
     files are traced.

     See the thread starting at
     <http://lists.gnu.org/archive/html/bug-bison/2008-07/msg00000.html>
     for details.  */
  {
    int i = 0;
    argv[i++] = m4;
    argv[i++] = "-I";
    argv[i++] = pkgdatadir;
    if (trace_flag & trace_m4)
      argv[i++] = "-dV";
    argv[i++] = full_m4sugar;
    argv[i++] = "-";
    argv[i++] = full_m4bison;
    argv[i++] = full_skeleton;
    argv[i++] = NULL;
  }
  /* When POSIXLY_CORRECT is set, some future versions of GNU M4 (most likely
     2.0) may drop some of the GNU extensions that Bison's skeletons depend
     upon.  So that the next release of Bison is forward compatible with those
     future versions of GNU M4, we unset POSIXLY_CORRECT here.

     FIXME: A user might set POSIXLY_CORRECT to affect processes run from
     macros like m4_syscmd in a custom skeleton.  For now, Bison makes no
     promises about the behavior of custom skeletons, so this scenario is not a
     concern.  However, we eventually want to eliminate this shortcoming.  The
     next release of GNU M4 (1.4.12 or 1.6) will accept the -g command-line
     option as a no-op, and later releases will accept it to indicate that
     POSIXLY_CORRECT should be ignored.  Once the GNU M4 versions that accept
     -g are pervasive, Bison should use -g instead of unsetting
     POSIXLY_CORRECT.

     See the thread starting at
     <http://lists.gnu.org/archive/html/bug-bison/2008-07/msg00000.html>
     for details.  */
  unsetenv ("POSIXLY_CORRECT");
  init_subpipe ();
  pid = create_subpipe (argv, filter_fd);
  free (full_m4sugar);
  free (full_m4bison);
  free (full_skeleton);

  out = fdopen (filter_fd[0], "w");
  if (! out)
    error (EXIT_FAILURE, get_errno (),
	   "fdopen");

  /* Output the definitions of all the muscles.  */
  fputs ("m4_init()\n", out);

  user_actions_output (out);
  merger_output (out);
  token_definitions_output (out);
  symbol_code_props_output (out, "destructors", &symbol_destructor_get);
  symbol_code_props_output (out, "printers", &symbol_printer_get);

  muscles_m4_output (out);
  xfclose (out);

  /* Read and process m4's output.  */
  timevar_push (TV_M4);
  end_of_output_subpipe (pid, filter_fd);
  in = fdopen (filter_fd[1], "r");
  if (! in)
    error (EXIT_FAILURE, get_errno (),
	   "fdopen");
  scan_skel (in);
  xfclose (in);
  reap_subpipe (pid, m4);
  timevar_pop (TV_M4);
}

static void
prepare (void)
{
  /* BISON_USE_PUSH_FOR_PULL is for the test suite and should not be documented
     for the user.  */
  char const *use_push_for_pull_env = getenv ("BISON_USE_PUSH_FOR_PULL");
  bool use_push_for_pull_flag = false;
  if (use_push_for_pull_env != NULL
      && use_push_for_pull_env[0] != '\0'
      && 0 != strcmp (use_push_for_pull_env, "0"))
    use_push_for_pull_flag = true;

  /* Flags. */
  MUSCLE_INSERT_BOOL ("debug_flag", debug_flag);
  MUSCLE_INSERT_BOOL ("defines_flag", defines_flag);
  MUSCLE_INSERT_BOOL ("error_verbose_flag", error_verbose);
  MUSCLE_INSERT_BOOL ("glr_flag", glr_parser);
  MUSCLE_INSERT_BOOL ("locations_flag", locations_flag);
  MUSCLE_INSERT_BOOL ("nondeterministic_flag", nondeterministic_parser);
  MUSCLE_INSERT_BOOL ("synclines_flag", !no_lines_flag);
  MUSCLE_INSERT_BOOL ("tag_seen_flag", tag_seen);
  MUSCLE_INSERT_BOOL ("use_push_for_pull_flag", use_push_for_pull_flag);
  MUSCLE_INSERT_BOOL ("yacc_flag", yacc_flag);

  /* File names.  */
  if (spec_name_prefix)
    MUSCLE_INSERT_STRING ("prefix", spec_name_prefix);

  MUSCLE_INSERT_STRING ("file_name_all_but_ext", all_but_ext);

#define DEFINE(Name) MUSCLE_INSERT_STRING (#Name, Name ? Name : "")
  DEFINE (dir_prefix);
  DEFINE (parser_file_name);
  DEFINE (spec_defines_file);
  DEFINE (spec_file_prefix);
  DEFINE (spec_graph_file);
  DEFINE (spec_name_prefix);
  DEFINE (spec_outfile);
  DEFINE (spec_verbose_file);
#undef DEFINE

  /* Find the right skeleton file, and add muscles about the skeletons.  */
  if (skeleton)
    MUSCLE_INSERT_C_STRING ("skeleton", skeleton);
  else
    skeleton = language->skeleton;

  /* About the skeletons.  */
  {
    /* b4_pkgdatadir is used inside m4_include in the skeletons, so digraphs
       would never be expanded.  Hopefully no one has M4-special characters in
       his Bison installation path.  */
    MUSCLE_INSERT_STRING_RAW ("pkgdatadir", compute_pkgdatadir ());
  }
}


/*----------------------------------------------------------.
| Output the parsing tables and the parser code to ftable.  |
`----------------------------------------------------------*/

void
output (void)
{
  obstack_init (&format_obstack);

  prepare_symbols ();
  prepare_rules ();
  prepare_states ();
  prepare_actions ();

  prepare ();

  /* Process the selected skeleton file.  */
  output_skeleton ();

  obstack_free (&format_obstack, NULL);
}

char const *
compute_pkgdatadir (void)
{
  char const *pkgdatadir = getenv ("BISON_PKGDATADIR");
  return pkgdatadir ? pkgdatadir : relocate (PKGDATADIR);
}
