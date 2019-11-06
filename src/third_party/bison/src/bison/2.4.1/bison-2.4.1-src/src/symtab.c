/* Symbol table manager for Bison.

   Copyright (C) 1984, 1989, 2000, 2001, 2002, 2004, 2005, 2006, 2007, 2008
   Free Software Foundation, Inc.

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

#include <hash.h>
#include <quotearg.h>

#include "complain.h"
#include "gram.h"
#include "symtab.h"

/*------------------------.
| Distinguished symbols.  |
`------------------------*/

symbol *errtoken = NULL;
symbol *undeftoken = NULL;
symbol *endtoken = NULL;
symbol *accept = NULL;
symbol *startsymbol = NULL;
location startsymbol_location;

/*---------------------------------------.
| Default %destructor's and %printer's.  |
`---------------------------------------*/

static code_props default_tagged_destructor = CODE_PROPS_NONE_INIT;
static code_props default_tagless_destructor = CODE_PROPS_NONE_INIT;
static code_props default_tagged_printer = CODE_PROPS_NONE_INIT;
static code_props default_tagless_printer = CODE_PROPS_NONE_INIT;

/*---------------------------------.
| Create a new symbol, named TAG.  |
`---------------------------------*/

static symbol *
symbol_new (uniqstr tag, location loc)
{
  symbol *res = xmalloc (sizeof *res);

  uniqstr_assert (tag);
  res->tag = tag;
  res->location = loc;

  res->type_name = NULL;
  code_props_none_init (&res->destructor);
  code_props_none_init (&res->printer);

  res->number = NUMBER_UNDEFINED;
  res->prec = 0;
  res->assoc = undef_assoc;
  res->user_token_number = USER_NUMBER_UNDEFINED;

  res->alias = NULL;
  res->class = unknown_sym;
  res->declared = false;

  if (nsyms == SYMBOL_NUMBER_MAXIMUM)
    fatal (_("too many symbols in input grammar (limit is %d)"),
	   SYMBOL_NUMBER_MAXIMUM);
  nsyms++;
  return res;
}

/*----------------------------------------.
| Create a new semantic type, named TAG.  |
`----------------------------------------*/

static semantic_type *
semantic_type_new (uniqstr tag)
{
  semantic_type *res = xmalloc (sizeof *res);

  uniqstr_assert (tag);
  res->tag = tag;
  code_props_none_init (&res->destructor);
  code_props_none_init (&res->printer);

  return res;
}


/*-----------------.
| Print a symbol.  |
`-----------------*/

#define SYMBOL_ATTR_PRINT(Attr)				\
  if (s->Attr)						\
    fprintf (f, " %s { %s }", #Attr, s->Attr)

#define SYMBOL_CODE_PRINT(Attr)                         \
  if (s->Attr.code)                                     \
    fprintf (f, " %s { %s }", #Attr, s->Attr.code)

void
symbol_print (symbol *s, FILE *f)
{
  if (s)
    {
      fprintf (f, "\"%s\"", s->tag);
      SYMBOL_ATTR_PRINT (type_name);
      SYMBOL_CODE_PRINT (destructor);
      SYMBOL_CODE_PRINT (printer);
    }
  else
    fprintf (f, "<NULL>");
}

#undef SYMBOL_ATTR_PRINT
#undef SYMBOL_CODE_PRINT

/*------------------------------------------------------------------.
| Complain that S's WHAT is redeclared at SECOND, and was first set |
| at FIRST.                                                         |
`------------------------------------------------------------------*/

static void
symbol_redeclaration (symbol *s, const char *what, location first,
                      location second)
{
  complain_at (second, _("%s redeclaration for %s"), what, s->tag);
  complain_at (first, _("previous declaration"));
}

static void
semantic_type_redeclaration (semantic_type *s, const char *what, location first,
                             location second)
{
  complain_at (second, _("%s redeclaration for <%s>"), what, s->tag);
  complain_at (first, _("previous declaration"));
}


/*-----------------------------------------------------------------.
| Set the TYPE_NAME associated with SYM.  Does nothing if passed 0 |
| as TYPE_NAME.                                                    |
`-----------------------------------------------------------------*/

void
symbol_type_set (symbol *sym, uniqstr type_name, location loc)
{
  if (type_name)
    {
      if (sym->type_name)
	symbol_redeclaration (sym, "%type", sym->type_location, loc);
      uniqstr_assert (type_name);
      sym->type_name = type_name;
      sym->type_location = loc;
    }
}

/*-----------------------------------.
| Get the CLASS associated with SYM. |
`-----------------------------------*/

const char *
symbol_class_get_string (symbol *sym)
{
  if (sym->class)
    {
      if (sym->class == token_sym)
	return "terminal";
      else if (sym->class == nterm_sym)
	return "nonterminal";
    }
  return "unknown";
}


/*-----------------------------------------.
| Set the DESTRUCTOR associated with SYM.  |
`-----------------------------------------*/

void
symbol_destructor_set (symbol *sym, code_props const *destructor)
{
  if (sym->destructor.code)
    symbol_redeclaration (sym, "%destructor", sym->destructor.location,
                          destructor->location);
  sym->destructor = *destructor;
}

/*------------------------------------------.
| Set the DESTRUCTOR associated with TYPE.  |
`------------------------------------------*/

void
semantic_type_destructor_set (semantic_type *type,
                              code_props const *destructor)
{
  if (type->destructor.code)
    semantic_type_redeclaration (type, "%destructor",
                                 type->destructor.location,
                                 destructor->location);
  type->destructor = *destructor;
}

/*---------------------------------------.
| Get the computed %destructor for SYM.  |
`---------------------------------------*/

code_props const *
symbol_destructor_get (symbol const *sym)
{
  /* Per-symbol %destructor.  */
  if (sym->destructor.code)
    return &sym->destructor;

  /* Per-type %destructor.  */
  if (sym->type_name)
    {
      code_props const *destructor =
        &semantic_type_get (sym->type_name)->destructor;
      if (destructor->code)
        return destructor;
    }

  /* Apply default %destructor's only to user-defined symbols.  */
  if (sym->tag[0] == '$' || sym == errtoken)
    return &code_props_none;

  if (sym->type_name)
    return &default_tagged_destructor;
  return &default_tagless_destructor;
}

/*--------------------------------------.
| Set the PRINTER associated with SYM.  |
`--------------------------------------*/

void
symbol_printer_set (symbol *sym, code_props const *printer)
{
  if (sym->printer.code)
    symbol_redeclaration (sym, "%printer",
                          sym->printer.location, printer->location);
  sym->printer = *printer;
}

/*---------------------------------------.
| Set the PRINTER associated with TYPE.  |
`---------------------------------------*/

void
semantic_type_printer_set (semantic_type *type, code_props const *printer)
{
  if (type->printer.code)
    semantic_type_redeclaration (type, "%printer",
                                 type->printer.location, printer->location);
  type->printer = *printer;
}

/*------------------------------------.
| Get the computed %printer for SYM.  |
`------------------------------------*/

code_props const *
symbol_printer_get (symbol const *sym)
{
  /* Per-symbol %printer.  */
  if (sym->printer.code)
    return &sym->printer;

  /* Per-type %printer.  */
  if (sym->type_name)
    {
      code_props const *printer = &semantic_type_get (sym->type_name)->printer;
      if (printer->code)
        return printer;
    }

  /* Apply the default %printer only to user-defined symbols.  */
  if (sym->tag[0] == '$' || sym == errtoken)
    return &code_props_none;

  if (sym->type_name)
    return &default_tagged_printer;
  return &default_tagless_printer;
}

/*-----------------------------------------------------------------.
| Set the PRECEDENCE associated with SYM.  Does nothing if invoked |
| with UNDEF_ASSOC as ASSOC.                                       |
`-----------------------------------------------------------------*/

void
symbol_precedence_set (symbol *sym, int prec, assoc a, location loc)
{
  if (a != undef_assoc)
    {
      if (sym->prec != 0)
	symbol_redeclaration (sym, assoc_to_string (a), sym->prec_location,
                              loc);
      sym->prec = prec;
      sym->assoc = a;
      sym->prec_location = loc;
    }

  /* Only terminals have a precedence. */
  symbol_class_set (sym, token_sym, loc, false);
}


/*------------------------------------.
| Set the CLASS associated with SYM.  |
`------------------------------------*/

void
symbol_class_set (symbol *sym, symbol_class class, location loc, bool declaring)
{
  if (sym->class != unknown_sym && sym->class != class)
    {
      complain_at (loc, _("symbol %s redefined"), sym->tag);
      sym->declared = false;
    }

  if (class == nterm_sym && sym->class != nterm_sym)
    sym->number = nvars++;
  else if (class == token_sym && sym->number == NUMBER_UNDEFINED)
    sym->number = ntokens++;

  sym->class = class;

  if (declaring)
    {
      if (sym->declared)
	warn_at (loc, _("symbol %s redeclared"), sym->tag);
      sym->declared = true;
    }
}


/*------------------------------------------------.
| Set the USER_TOKEN_NUMBER associated with SYM.  |
`------------------------------------------------*/

void
symbol_user_token_number_set (symbol *sym, int user_token_number, location loc)
{
  int *user_token_numberp;

  if (sym->user_token_number != USER_NUMBER_ALIAS)
    user_token_numberp = &sym->user_token_number;
  else
    user_token_numberp = &sym->alias->user_token_number;
  if (*user_token_numberp != USER_NUMBER_UNDEFINED
      && *user_token_numberp != user_token_number)
    complain_at (loc, _("redefining user token number of %s"), sym->tag);

  *user_token_numberp = user_token_number;
  /* User defined $end token? */
  if (user_token_number == 0)
    {
      endtoken = sym;
      endtoken->number = 0;
      /* It is always mapped to 0, so it was already counted in
	 NTOKENS.  */
      --ntokens;
    }
}


/*----------------------------------------------------------.
| If SYM is not defined, report an error, and consider it a |
| nonterminal.                                              |
`----------------------------------------------------------*/

static inline bool
symbol_check_defined (symbol *sym)
{
  if (sym->class == unknown_sym)
    {
      complain_at
	(sym->location,
	 _("symbol %s is used, but is not defined as a token and has no rules"),
	 sym->tag);
      sym->class = nterm_sym;
      sym->number = nvars++;
    }

  return true;
}

static bool
symbol_check_defined_processor (void *sym, void *null ATTRIBUTE_UNUSED)
{
  return symbol_check_defined (sym);
}


/*------------------------------------------------------------------.
| Declare the new symbol SYM.  Make it an alias of SYMVAL, and type |
| SYMVAL with SYM's type.                                           |
`------------------------------------------------------------------*/

void
symbol_make_alias (symbol *sym, symbol *symval, location loc)
{
  if (symval->alias)
    warn_at (loc, _("symbol `%s' used more than once as a literal string"),
	     symval->tag);
  else if (sym->alias)
    warn_at (loc, _("symbol `%s' given more than one literal string"),
	     sym->tag);
  else
    {
      symval->class = token_sym;
      symval->user_token_number = sym->user_token_number;
      sym->user_token_number = USER_NUMBER_ALIAS;
      symval->alias = sym;
      sym->alias = symval;
      symval->number = sym->number;
      symbol_type_set (symval, sym->type_name, loc);
    }
}


/*---------------------------------------------------------.
| Check that THIS, and its alias, have same precedence and |
| associativity.                                           |
`---------------------------------------------------------*/

static inline void
symbol_check_alias_consistency (symbol *this)
{
  symbol *alias = this;
  symbol *orig  = this->alias;

  /* Check only those that _are_ the aliases.  */
  if (!(this->alias && this->user_token_number == USER_NUMBER_ALIAS))
    return;

  if (orig->type_name != alias->type_name)
    {
      if (orig->type_name)
	symbol_type_set (alias, orig->type_name, orig->type_location);
      else
	symbol_type_set (orig, alias->type_name, alias->type_location);
    }


  if (orig->destructor.code || alias->destructor.code)
    {
      if (orig->destructor.code)
	symbol_destructor_set (alias, &orig->destructor);
      else
	symbol_destructor_set (orig, &alias->destructor);
    }

  if (orig->printer.code || alias->printer.code)
    {
      if (orig->printer.code)
	symbol_printer_set (alias, &orig->printer);
      else
	symbol_printer_set (orig, &alias->printer);
    }

  if (alias->prec || orig->prec)
    {
      if (orig->prec)
	symbol_precedence_set (alias, orig->prec, orig->assoc,
			       orig->prec_location);
      else
	symbol_precedence_set (orig, alias->prec, alias->assoc,
			       alias->prec_location);
    }
}

static bool
symbol_check_alias_consistency_processor (void *this,
					  void *null ATTRIBUTE_UNUSED)
{
  symbol_check_alias_consistency (this);
  return true;
}


/*-------------------------------------------------------------------.
| Assign a symbol number, and write the definition of the token name |
| into FDEFINES.  Put in SYMBOLS.                                    |
`-------------------------------------------------------------------*/

static inline bool
symbol_pack (symbol *this)
{
  if (this->class == nterm_sym)
    {
      this->number += ntokens;
    }
  else if (this->alias)
    {
      /* This symbol and its alias are a single token defn.
	 Allocate a tokno, and assign to both check agreement of
	 prec and assoc fields and make both the same */
      if (this->number == NUMBER_UNDEFINED)
	{
	  if (this == endtoken || this->alias == endtoken)
	    this->number = this->alias->number = 0;
	  else
	    {
	      aver (this->alias->number != NUMBER_UNDEFINED);
	      this->number = this->alias->number;
	    }
	}
      /* Do not do processing below for USER_NUMBER_ALIASes.  */
      if (this->user_token_number == USER_NUMBER_ALIAS)
	return true;
    }
  else /* this->class == token_sym */
    aver (this->number != NUMBER_UNDEFINED);

  symbols[this->number] = this;
  return true;
}

static bool
symbol_pack_processor (void *this, void *null ATTRIBUTE_UNUSED)
{
  return symbol_pack (this);
}




/*--------------------------------------------------.
| Put THIS in TOKEN_TRANSLATIONS if it is a token.  |
`--------------------------------------------------*/

static inline bool
symbol_translation (symbol *this)
{
  /* Non-terminal? */
  if (this->class == token_sym
      && this->user_token_number != USER_NUMBER_ALIAS)
    {
      /* A token which translation has already been set? */
      if (token_translations[this->user_token_number] != undeftoken->number)
	complain_at (this->location,
		     _("tokens %s and %s both assigned number %d"),
		     symbols[token_translations[this->user_token_number]]->tag,
		     this->tag, this->user_token_number);

      token_translations[this->user_token_number] = this->number;
    }

  return true;
}

static bool
symbol_translation_processor (void *this, void *null ATTRIBUTE_UNUSED)
{
  return symbol_translation (this);
}


/*---------------------------------------.
| Symbol and semantic type hash tables.  |
`---------------------------------------*/

/* Initial capacity of symbol and semantic type hash table.  */
#define HT_INITIAL_CAPACITY 257

static struct hash_table *symbol_table = NULL;
static struct hash_table *semantic_type_table = NULL;

static inline bool
hash_compare_symbol (const symbol *m1, const symbol *m2)
{
  /* Since tags are unique, we can compare the pointers themselves.  */
  return UNIQSTR_EQ (m1->tag, m2->tag);
}

static inline bool
hash_compare_semantic_type (const semantic_type *m1, const semantic_type *m2)
{
  /* Since names are unique, we can compare the pointers themselves.  */
  return UNIQSTR_EQ (m1->tag, m2->tag);
}

static bool
hash_symbol_comparator (void const *m1, void const *m2)
{
  return hash_compare_symbol (m1, m2);
}

static bool
hash_semantic_type_comparator (void const *m1, void const *m2)
{
  return hash_compare_semantic_type (m1, m2);
}

static inline size_t
hash_symbol (const symbol *m, size_t tablesize)
{
  /* Since tags are unique, we can hash the pointer itself.  */
  return ((uintptr_t) m->tag) % tablesize;
}

static inline size_t
hash_semantic_type (const semantic_type *m, size_t tablesize)
{
  /* Since names are unique, we can hash the pointer itself.  */
  return ((uintptr_t) m->tag) % tablesize;
}

static size_t
hash_symbol_hasher (void const *m, size_t tablesize)
{
  return hash_symbol (m, tablesize);
}

static size_t
hash_semantic_type_hasher (void const *m, size_t tablesize)
{
  return hash_semantic_type (m, tablesize);
}

/*-------------------------------.
| Create the symbol hash table.  |
`-------------------------------*/

void
symbols_new (void)
{
  symbol_table = hash_initialize (HT_INITIAL_CAPACITY,
				  NULL,
				  hash_symbol_hasher,
				  hash_symbol_comparator,
				  free);
  semantic_type_table = hash_initialize (HT_INITIAL_CAPACITY,
				         NULL,
				         hash_semantic_type_hasher,
				         hash_semantic_type_comparator,
				         free);
}


/*----------------------------------------------------------------.
| Find the symbol named KEY, and return it.  If it does not exist |
| yet, create it.                                                 |
`----------------------------------------------------------------*/

symbol *
symbol_from_uniqstr (const uniqstr key, location loc)
{
  symbol probe;
  symbol *entry;

  probe.tag = key;
  entry = hash_lookup (symbol_table, &probe);

  if (!entry)
    {
      /* First insertion in the hash. */
      entry = symbol_new (key, loc);
      hash_insert (symbol_table, entry);
    }
  return entry;
}


/*-----------------------------------------------------------------------.
| Find the semantic type named KEY, and return it.  If it does not exist |
| yet, create it.                                                        |
`-----------------------------------------------------------------------*/

semantic_type *
semantic_type_from_uniqstr (const uniqstr key)
{
  semantic_type probe;
  semantic_type *entry;

  probe.tag = key;
  entry = hash_lookup (semantic_type_table, &probe);

  if (!entry)
    {
      /* First insertion in the hash. */
      entry = semantic_type_new (key);
      hash_insert (semantic_type_table, entry);
    }
  return entry;
}


/*----------------------------------------------------------------.
| Find the symbol named KEY, and return it.  If it does not exist |
| yet, create it.                                                 |
`----------------------------------------------------------------*/

symbol *
symbol_get (const char *key, location loc)
{
  return symbol_from_uniqstr (uniqstr_new (key), loc);
}


/*-----------------------------------------------------------------------.
| Find the semantic type named KEY, and return it.  If it does not exist |
| yet, create it.                                                        |
`-----------------------------------------------------------------------*/

semantic_type *
semantic_type_get (const char *key)
{
  return semantic_type_from_uniqstr (uniqstr_new (key));
}


/*------------------------------------------------------------------.
| Generate a dummy nonterminal, whose name cannot conflict with the |
| user's names.                                                     |
`------------------------------------------------------------------*/

symbol *
dummy_symbol_get (location loc)
{
  /* Incremented for each generated symbol.  */
  static int dummy_count = 0;
  static char buf[256];

  symbol *sym;

  sprintf (buf, "$@%d", ++dummy_count);
  sym = symbol_get (buf, loc);
  sym->class = nterm_sym;
  sym->number = nvars++;
  return sym;
}

bool
symbol_is_dummy (const symbol *sym)
{
  return sym->tag[0] == '@' || (sym->tag[0] == '$' && sym->tag[1] == '@');
}

/*-------------------.
| Free the symbols.  |
`-------------------*/

void
symbols_free (void)
{
  hash_free (symbol_table);
  hash_free (semantic_type_table);
  free (symbols);
}


/*---------------------------------------------------------------.
| Look for undefined symbols, report an error, and consider them |
| terminals.                                                     |
`---------------------------------------------------------------*/

static void
symbols_do (Hash_processor processor, void *processor_data)
{
  hash_do_for_each (symbol_table, processor, processor_data);
}


/*--------------------------------------------------------------.
| Check that all the symbols are defined.  Report any undefined |
| symbols and consider them nonterminals.                       |
`--------------------------------------------------------------*/

void
symbols_check_defined (void)
{
  symbols_do (symbol_check_defined_processor, NULL);
}

/*------------------------------------------------------------------.
| Set TOKEN_TRANSLATIONS.  Check that no two symbols share the same |
| number.                                                           |
`------------------------------------------------------------------*/

static void
symbols_token_translations_init (void)
{
  bool num_256_available_p = true;
  int i;

  /* Find the highest user token number, and whether 256, the POSIX
     preferred user token number for the error token, is used.  */
  max_user_token_number = 0;
  for (i = 0; i < ntokens; ++i)
    {
      symbol *this = symbols[i];
      if (this->user_token_number != USER_NUMBER_UNDEFINED)
	{
	  if (this->user_token_number > max_user_token_number)
	    max_user_token_number = this->user_token_number;
	  if (this->user_token_number == 256)
	    num_256_available_p = false;
	}
    }

  /* If 256 is not used, assign it to error, to follow POSIX.  */
  if (num_256_available_p
      && errtoken->user_token_number == USER_NUMBER_UNDEFINED)
    errtoken->user_token_number = 256;

  /* Set the missing user numbers. */
  if (max_user_token_number < 256)
    max_user_token_number = 256;

  for (i = 0; i < ntokens; ++i)
    {
      symbol *this = symbols[i];
      if (this->user_token_number == USER_NUMBER_UNDEFINED)
	this->user_token_number = ++max_user_token_number;
      if (this->user_token_number > max_user_token_number)
	max_user_token_number = this->user_token_number;
    }

  token_translations = xnmalloc (max_user_token_number + 1,
				 sizeof *token_translations);

  /* Initialize all entries for literal tokens to 2, the internal
     token number for $undefined, which represents all invalid inputs.
     */
  for (i = 0; i < max_user_token_number + 1; i++)
    token_translations[i] = undeftoken->number;
  symbols_do (symbol_translation_processor, NULL);
}


/*----------------------------------------------------------------.
| Assign symbol numbers, and write definition of token names into |
| FDEFINES.  Set up vectors SYMBOL_TABLE, TAGS of symbols.        |
`----------------------------------------------------------------*/

void
symbols_pack (void)
{
  symbols_do (symbol_check_alias_consistency_processor, NULL);

  symbols = xcalloc (nsyms, sizeof *symbols);
  symbols_do (symbol_pack_processor, NULL);

  /* Aliases leave empty slots in symbols, so remove them.  */
  {
    int writei;
    int readi;
    int nsyms_old = nsyms;
    for (writei = 0, readi = 0; readi < nsyms_old; readi += 1)
      {
        if (symbols[readi] == NULL)
          {
            nsyms -= 1;
            ntokens -= 1;
          }
        else
          {
            symbols[writei] = symbols[readi];
            symbols[writei]->number = writei;
            if (symbols[writei]->alias)
              symbols[writei]->alias->number = writei;
            writei += 1;
          }
      }
  }
  symbols = xnrealloc (symbols, nsyms, sizeof *symbols);

  symbols_token_translations_init ();

  if (startsymbol->class == unknown_sym)
    fatal_at (startsymbol_location,
	      _("the start symbol %s is undefined"),
	      startsymbol->tag);
  else if (startsymbol->class == token_sym)
    fatal_at (startsymbol_location,
	      _("the start symbol %s is a token"),
	      startsymbol->tag);
}


/*--------------------------------------------------.
| Set default tagged/tagless %destructor/%printer.  |
`--------------------------------------------------*/

void
default_tagged_destructor_set (code_props const *destructor)
{
  if (default_tagged_destructor.code)
    {
      complain_at (destructor->location,
                   _("redeclaration for default tagged %%destructor"));
      complain_at (default_tagged_destructor.location,
		   _("previous declaration"));
    }
  default_tagged_destructor = *destructor;
}

void
default_tagless_destructor_set (code_props const *destructor)
{
  if (default_tagless_destructor.code)
    {
      complain_at (destructor->location,
                   _("redeclaration for default tagless %%destructor"));
      complain_at (default_tagless_destructor.location,
		   _("previous declaration"));
    }
  default_tagless_destructor = *destructor;
}

void
default_tagged_printer_set (code_props const *printer)
{
  if (default_tagged_printer.code)
    {
      complain_at (printer->location,
                   _("redeclaration for default tagged %%printer"));
      complain_at (default_tagged_printer.location,
		   _("previous declaration"));
    }
  default_tagged_printer = *printer;
}

void
default_tagless_printer_set (code_props const *printer)
{
  if (default_tagless_printer.code)
    {
      complain_at (printer->location,
                   _("redeclaration for default tagless %%printer"));
      complain_at (default_tagless_printer.location,
		   _("previous declaration"));
    }
  default_tagless_printer = *printer;
}
