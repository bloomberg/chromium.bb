/* Parse command line arguments for Bison.

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
#include "output.h"

#include <argmatch.h>
#include <c-strcase.h>
#include <configmake.h>
#include <error.h>

/* Hack to get <getopt.h> to declare getopt with a prototype.  */
#if lint && ! defined __GNU_LIBRARY__
# define __GNU_LIBRARY__
# define HACK_FOR___GNU_LIBRARY___PROTOTYPE 1
#endif

#include <getopt.h>

#ifdef HACK_FOR___GNU_LIBRARY___PROTOTYPE
# undef __GNU_LIBRARY__
# undef HACK_FOR___GNU_LIBRARY___PROTOTYPE
#endif

#include "complain.h"
#include "files.h"
#include "getargs.h"
#include "uniqstr.h"

bool debug_flag;
bool defines_flag;
bool graph_flag;
bool xml_flag;
bool locations_flag;
bool no_lines_flag;
bool token_table_flag;
bool yacc_flag;	/* for -y */

bool error_verbose = false;

bool nondeterministic_parser = false;
bool glr_parser = false;

int report_flag = report_none;
int trace_flag = trace_none;
int warnings_flag = warnings_none;

static struct bison_language const valid_languages[] = {
  { "c", "c-skel.m4", ".c", ".h", true },
  { "c++", "c++-skel.m4", ".cc", ".hh", true },
  { "java", "java-skel.m4", ".java", ".java", false },
  { "", "", "", "", false }
};

int skeleton_prio = default_prio;
const char *skeleton = NULL;
int language_prio = default_prio;
struct bison_language const *language = &valid_languages[0];
const char *include = NULL;


/** Decode an option's set of keys.
 *
 *  \param option   option being decoded.
 *  \param keys     array of valid subarguments.
 *  \param values   array of corresponding (int) values.
 *  \param flags    the flags to update
 *  \param args     colon separated list of effective subarguments to decode.
 *                  If 0, then activate all the flags.
 *
 *  The special value 0 resets the flags to 0.
 */
static void
flags_argmatch (const char *option,
		const char * const keys[], const int values[],
		int *flags, char *args)
{
  if (args)
    {
      args = strtok (args, ",");
      while (args)
	{
	  int value = XARGMATCH (option, args, keys, values);
	  if (value == 0)
	    *flags = 0;
	  else
	    *flags |= value;
	  args = strtok (NULL, ",");
	}
    }
  else
    *flags = ~0;
}

/** Decode a set of sub arguments.
 *
 *  \param FlagName  the flag familly to update.
 *  \param Args      the effective sub arguments to decode.
 *
 *  \arg FlagName_args   the list of keys.
 *  \arg FlagName_types  the list of values.
 *  \arg FlagName_flag   the flag to update.
 */
#define FLAGS_ARGMATCH(FlagName, Args)					\
  flags_argmatch ("--" #FlagName, FlagName ## _args, FlagName ## _types, \
		  &FlagName ## _flag, Args)


/*----------------------.
| --report's handling.  |
`----------------------*/

static const char * const report_args[] =
{
  /* In a series of synonyms, present the most meaningful first, so
     that argmatch_valid be more readable.  */
  "none",
  "state", "states",
  "itemset", "itemsets",
  "lookahead", "lookaheads", "look-ahead",
  "solved",
  "all",
  0
};

static const int report_types[] =
{
  report_none,
  report_states, report_states,
  report_states | report_itemsets, report_states | report_itemsets,
  report_states | report_lookahead_tokens,
  report_states | report_lookahead_tokens,
  report_states | report_lookahead_tokens,
  report_states | report_solved_conflicts,
  report_all
};

ARGMATCH_VERIFY (report_args, report_types);


/*---------------------.
| --trace's handling.  |
`---------------------*/

static const char * const trace_args[] =
{
  /* In a series of synonyms, present the most meaningful first, so
     that argmatch_valid be more readable.  */
  "none       - no traces",
  "scan       - grammar scanner traces",
  "parse      - grammar parser traces",
  "automaton  - construction of the automaton",
  "bitsets    - use of bitsets",
  "grammar    - reading, reducing the grammar",
  "resource   - memory consumption (where available)",
  "sets       - grammar sets: firsts, nullable etc.",
  "tools      - m4 invocation",
  "m4         - m4 traces",
  "skeleton   - skeleton postprocessing",
  "time       - time consumption",
  "all        - all of the above",
  0
};

static const int trace_types[] =
{
  trace_none,
  trace_scan,
  trace_parse,
  trace_automaton,
  trace_bitsets,
  trace_grammar,
  trace_resource,
  trace_sets,
  trace_tools,
  trace_m4,
  trace_skeleton,
  trace_time,
  trace_all
};

ARGMATCH_VERIFY (trace_args, trace_types);


/*------------------------.
| --warnings's handling.  |
`------------------------*/

static const char * const warnings_args[] =
{
  /* In a series of synonyms, present the most meaningful first, so
     that argmatch_valid be more readable.  */
  "none            - no warnings",
  "midrule-values  - unset or unused midrule values",
  "yacc            - incompatibilities with POSIX YACC",
  "all             - all of the above",
  "error           - warnings are errors",
  0
};

static const int warnings_types[] =
{
  warnings_none,
  warnings_midrule_values,
  warnings_yacc,
  warnings_all,
  warnings_error
};

ARGMATCH_VERIFY (warnings_args, warnings_types);


/*-------------------------------------------.
| Display the help message and exit STATUS.  |
`-------------------------------------------*/

static void usage (int) ATTRIBUTE_NORETURN;

static void
usage (int status)
{
  if (status != 0)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
  else
    {
      printf (_("Usage: %s [OPTION]... FILE\n"), program_name);
      fputs (_("\
Generate LALR(1) and GLR parsers.\n\
\n\
"), stdout);

      fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
      fputs (_("\
The same is true for optional arguments.\n\
"), stdout);

      fputs (_("\
\n\
Operation modes:\n\
  -h, --help                 display this help and exit\n\
  -V, --version              output version information and exit\n\
      --print-localedir      output directory containing locale-dependent data\n\
      --print-datadir        output directory containing skeletons and XSLT\n\
  -y, --yacc                 emulate POSIX Yacc\n\
  -W, --warnings=[CATEGORY]  report the warnings falling in CATEGORY\n\
\n\
"), stdout);

      fputs (_("\
Parser:\n\
  -L, --language=LANGUAGE    specify the output programming language\n\
                             (this is an experimental feature)\n\
  -S, --skeleton=FILE        specify the skeleton to use\n\
  -t, --debug                instrument the parser for debugging\n\
      --locations            enable locations computation\n\
  -p, --name-prefix=PREFIX   prepend PREFIX to the external symbols\n\
  -l, --no-lines             don't generate `#line' directives\n\
  -k, --token-table          include a table of token names\n\
\n\
"), stdout);

      /* Keep -d and --defines separate so that ../build-aux/cross-options.pl
       * won't assume that -d also takes an argument.  */
      fputs (_("\
Output:\n\
      --defines[=FILE]       also produce a header file\n\
  -d                         likewise but cannot specify FILE (for POSIX Yacc)\n\
  -r, --report=THINGS        also produce details on the automaton\n\
      --report-file=FILE     write report to FILE\n\
  -v, --verbose              same as `--report=state'\n\
  -b, --file-prefix=PREFIX   specify a PREFIX for output files\n\
  -o, --output=FILE          leave output to FILE\n\
  -g, --graph[=FILE]         also output a graph of the automaton\n\
  -x, --xml[=FILE]           also output an XML report of the automaton\n\
                             (the XML schema is experimental)\n\
\n\
"), stdout);

      fputs (_("\
Warning categories include:\n\
  `midrule-values'  unset or unused midrule values\n\
  `yacc'            incompatibilities with POSIX YACC\n\
  `all'             all the warnings\n\
  `no-CATEGORY'     turn off warnings in CATEGORY\n\
  `none'            turn off all the warnings\n\
  `error'           treat warnings as errors\n\
\n\
"), stdout);

      fputs (_("\
THINGS is a list of comma separated words that can include:\n\
  `state'        describe the states\n\
  `itemset'      complete the core item sets with their closure\n\
  `lookahead'    explicitly associate lookahead tokens to items\n\
  `solved'       describe shift/reduce conflicts solving\n\
  `all'          include all the above information\n\
  `none'         disable the report\n\
"), stdout);

      printf (_("\nReport bugs to <%s>.\n"), PACKAGE_BUGREPORT);
    }

  exit (status);
}


/*------------------------------.
| Display the version message.  |
`------------------------------*/

static void
version (void)
{
  /* Some efforts were made to ease the translators' task, please
     continue.  */
  printf (_("bison (GNU Bison) %s"), VERSION);
  putc ('\n', stdout);
  fputs (_("Written by Robert Corbett and Richard Stallman.\n"), stdout);
  putc ('\n', stdout);

  fprintf (stdout,
	   _("Copyright (C) %d Free Software Foundation, Inc.\n"),
	   PACKAGE_COPYRIGHT_YEAR);

  fputs (_("\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"),
	 stdout);
}


/*-------------------------------------.
| --skeleton and --language handling.  |
`--------------------------------------*/

void
skeleton_arg (char const *arg, int prio, location const *loc)
{
  if (prio < skeleton_prio)
    {
      skeleton_prio = prio;
      skeleton = arg;
    }
  else if (prio == skeleton_prio)
    {
      char const *msg =
	_("multiple skeleton declarations are invalid");
      if (loc)
	complain_at (*loc, msg);
      else
	complain (msg);
    }
}

void
language_argmatch (char const *arg, int prio, location const *loc)
{
  char const *msg;

  if (prio < language_prio)
    {
      int i;
      for (i = 0; valid_languages[i].language[0]; i++)
	if (c_strcasecmp (arg, valid_languages[i].language) == 0)
	  {
	    language_prio = prio;
	    language = &valid_languages[i];
	    return;
	  }
      msg = _("invalid language `%s'");
    }
  else if (language_prio == prio)
    msg = _("multiple language declarations are invalid");
  else
    return;

  if (loc)
    complain_at (*loc, msg, arg);
  else
    complain (msg, arg);
}

/*----------------------.
| Process the options.  |
`----------------------*/

/* Shorts options.
   Should be computed from long_options.  */
static char const short_options[] =
  "L:"
  "S:"
  "T::"
  "V"
  "W::"
  "b:"
  "d"
  "e"
  "g::"
  "h"
  "k"
  "l"
  "n"
  "o:"
  "p:"
  "r:"
  "t"
  "v"
  "x::"
  "y"
  ;

/* Values for long options that do not have single-letter equivalents.  */
enum
{
  LOCATIONS_OPTION = CHAR_MAX + 1,
  PRINT_LOCALEDIR_OPTION,
  PRINT_DATADIR_OPTION,
  REPORT_FILE_OPTION
};

static struct option const long_options[] =
{
  /* Operation modes. */
  { "help",            no_argument,	  0,   'h' },
  { "version",         no_argument,	  0,   'V' },
  { "print-localedir", no_argument,	  0,   PRINT_LOCALEDIR_OPTION },
  { "print-datadir",   no_argument,	  0,   PRINT_DATADIR_OPTION   },
  { "warnings",        optional_argument, 0,   'W' },

  /* Parser. */
  { "name-prefix",   required_argument,	  0,   'p' },
  { "include",       required_argument,   0,   'I' },

  /* Output. */
  { "file-prefix", required_argument,	0,   'b' },
  { "output",	   required_argument,	0,   'o' },
  { "output-file", required_argument,	0,   'o' },
  { "graph",	   optional_argument,	0,   'g' },
  { "xml",         optional_argument,   0,   'x' },
  { "report",	   required_argument,   0,   'r' },
  { "report-file", required_argument,   0,   REPORT_FILE_OPTION },
  { "verbose",	   no_argument,	        0,   'v' },

  /* Hidden. */
  { "trace",         optional_argument,   0,     'T' },

  /* Output.  */
  { "defines",     optional_argument,   0,   'd' },

  /* Operation modes.  */
  { "fixed-output-files", no_argument,  0,   'y' },
  { "yacc",	          no_argument,  0,   'y' },

  /* Parser.  */
  { "debug",	      no_argument,               0,   't' },
  { "locations",      no_argument,		 0, LOCATIONS_OPTION },
  { "no-lines",       no_argument,               0,   'l' },
  { "raw",            no_argument,               0,     0 },
  { "skeleton",       required_argument,         0,   'S' },
  { "language",       required_argument,         0,   'L' },
  { "token-table",    no_argument,               0,   'k' },

  {0, 0, 0, 0}
};

/* Under DOS, there is no difference on the case.  This can be
   troublesome when looking for `.tab' etc.  */
#ifdef MSDOS
# define AS_FILE_NAME(File) (strlwr (File), (File))
#else
# define AS_FILE_NAME(File) (File)
#endif

void
getargs (int argc, char *argv[])
{
  int c;

  while ((c = getopt_long (argc, argv, short_options, long_options, NULL))
	 != -1)
    switch (c)
      {
      case 0:
	/* Certain long options cause getopt_long to return 0.  */
	break;

      case 'd':
	/* Here, the -d and --defines options are differentiated.  */
	defines_flag = true;
	if (optarg)
	  spec_defines_file = xstrdup (AS_FILE_NAME (optarg));
	break;

      case 'I':
	include = AS_FILE_NAME (optarg);
	break;

      case 'L':
	language_argmatch (optarg, command_line_prio, NULL);
	break;

      case 'S':
	skeleton_arg (AS_FILE_NAME (optarg), command_line_prio, NULL);
	break;

      case 'T':
	FLAGS_ARGMATCH (trace, optarg);
	break;

      case 'V':
	version ();
	exit (EXIT_SUCCESS);

      case 'W':
	if (optarg)
	  FLAGS_ARGMATCH (warnings, optarg);
	else
	  warnings_flag |= warnings_all;
	break;

      case 'b':
	spec_file_prefix = AS_FILE_NAME (optarg);
	break;

      case 'g':
	graph_flag = true;
	if (optarg)
	  spec_graph_file = xstrdup (AS_FILE_NAME (optarg));
	break;

      case 'h':
	usage (EXIT_SUCCESS);

      case 'k':
	token_table_flag = true;
	break;

      case 'l':
	no_lines_flag = true;
	break;

      case 'o':
	spec_outfile = AS_FILE_NAME (optarg);
	break;

      case 'p':
	spec_name_prefix = optarg;
	break;

      case 'r':
	FLAGS_ARGMATCH (report, optarg);
	break;

      case 't':
	debug_flag = true;
	break;

      case 'v':
	report_flag |= report_states;
	break;

      case 'x':
	xml_flag = true;
	if (optarg)
	  spec_xml_file = xstrdup (AS_FILE_NAME (optarg));
	break;

      case 'y':
	yacc_flag = true;
	break;

      case LOCATIONS_OPTION:
	locations_flag = true;
	break;

      case PRINT_LOCALEDIR_OPTION:
	printf ("%s\n", LOCALEDIR);
	exit (EXIT_SUCCESS);

      case PRINT_DATADIR_OPTION:
	printf ("%s\n", compute_pkgdatadir ());
	exit (EXIT_SUCCESS);

      case REPORT_FILE_OPTION:
	spec_verbose_file = xstrdup (AS_FILE_NAME (optarg));
	break;

      default:
	usage (EXIT_FAILURE);
      }

  if (argc - optind != 1)
    {
      if (argc - optind < 1)
	error (0, 0, _("missing operand after `%s'"), argv[argc - 1]);
      else
	error (0, 0, _("extra operand `%s'"), argv[optind + 1]);
      usage (EXIT_FAILURE);
    }

  current_file = grammar_file = uniqstr_new (argv[optind]);
}
