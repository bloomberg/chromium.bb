/*
 * $XFree86: $
 *
 * Copyright © 2002 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdarg.h>
#include "fcint.h"

static xmlParserInputPtr
FcEntityLoader (const char *url, const char *id, xmlParserCtxtPtr ctxt)
{
    xmlParserInputPtr	ret;
    char		*file;

    file = FcConfigFilename (url);
    if (!file)
	return 0;
    ret = xmlNewInputFromFile (ctxt, file);
    free (file);
    return ret;
}

xmlDocPtr
FcConfigLoad (const char *file)
{
    xmlDocPtr		    doc;
    xmlExternalEntityLoader previous;

    previous = xmlGetExternalEntityLoader ();
    xmlSetExternalEntityLoader (FcEntityLoader);
    doc = xmlParseFile (file);
    xmlSetExternalEntityLoader (previous);
    return doc;
}

#if 0
int
FcConfigSave (char *file, xmlDocPtr doc)
{
}
#endif

FcTest *
FcTestCreate (FcQual qual, const char *field, FcOp compare, FcExpr *expr)
{
    FcTest	*test = (FcTest *) malloc (sizeof (FcTest));;

    if (test)
    {
	test->next = 0;
	test->qual = qual;
	test->field = FcStrCopy (field);
	test->op = compare;
	test->expr = expr;
    }
    return test;
}

void
FcTestDestroy (FcTest *test)
{
    if (test->next)
	FcTestDestroy (test->next);
    FcExprDestroy (test->expr);
    FcStrFree ((FcChar8 *) test->field);
    free (test);
}

FcExpr *
FcExprCreateInteger (int i)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpInteger;
	e->u.ival = i;
    }
    return e;
}

FcExpr *
FcExprCreateDouble (double d)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpDouble;
	e->u.dval = d;
    }
    return e;
}

FcExpr *
FcExprCreateString (const char *s)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpString;
	e->u.sval = FcStrCopy (s);
    }
    return e;
}

FcExpr *
FcExprCreateMatrix (const FcMatrix *m)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpMatrix;
	e->u.mval = FcMatrixCopy (m);
    }
    return e;
}

FcExpr *
FcExprCreateBool (FcBool b)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpBool;
	e->u.bval = b;
    }
    return e;
}

FcExpr *
FcExprCreateNil (void)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpNil;
    }
    return e;
}

FcExpr *
FcExprCreateField (const char *field)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpField;
	e->u.field = FcStrCopy (field);
    }
    return e;
}

FcExpr *
FcExprCreateConst (const char *constant)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpConst;
	e->u.constant = FcStrCopy (constant);
    }
    return e;
}

FcExpr *
FcExprCreateOp (FcExpr *left, FcOp op, FcExpr *right)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = op;
	e->u.tree.left = left;
	e->u.tree.right = right;
    }
    return e;
}

void
FcExprDestroy (FcExpr *e)
{
    switch (e->op) {
    case FcOpInteger:
	break;
    case FcOpDouble:
	break;
    case FcOpString:
	FcStrFree (e->u.sval);
	break;
    case FcOpMatrix:
	FcMatrixFree (e->u.mval);
	break;
    case FcOpCharSet:
	FcCharSetDestroy (e->u.cval);
	break;
    case FcOpBool:
	break;
    case FcOpField:
	FcStrFree (e->u.field);
	break;
    case FcOpConst:
	FcStrFree (e->u.constant);
	break;
    case FcOpAssign:
    case FcOpAssignReplace:
    case FcOpPrepend:
    case FcOpPrependFirst:
    case FcOpAppend:
    case FcOpAppendLast:
	break;
    case FcOpOr:
    case FcOpAnd:
    case FcOpEqual:
    case FcOpContains:
    case FcOpNotEqual:
    case FcOpLess:
    case FcOpLessEqual:
    case FcOpMore:
    case FcOpMoreEqual:
    case FcOpPlus:
    case FcOpMinus:
    case FcOpTimes:
    case FcOpDivide:
    case FcOpQuest:
    case FcOpComma:
	FcExprDestroy (e->u.tree.right);
	/* fall through */
    case FcOpNot:
	FcExprDestroy (e->u.tree.left);
	break;
    case FcOpNil:
    case FcOpInvalid:
	break;
    }
    free (e);
}

FcEdit *
FcEditCreate (const char *field, FcOp op, FcExpr *expr)
{
    FcEdit *e = (FcEdit *) malloc (sizeof (FcEdit));

    if (e)
    {
	e->next = 0;
	e->field = field;   /* already saved in grammar */
	e->op = op;
	e->expr = expr;
    }
    return e;
}

void
FcEditDestroy (FcEdit *e)
{
    if (e->next)
	FcEditDestroy (e->next);
    FcStrFree ((FcChar8 *) e->field);
    if (e->expr)
	FcExprDestroy (e->expr);
}

char *
FcConfigSaveField (const char *field)
{
    return FcStrCopy (field);
}

static void
FcConfigParseError (char *fmt, ...)
{
    va_list	args;

    va_start (args, fmt);
    fprintf (stderr, "font configuration error: ");
    vfprintf (stderr, fmt, args);
    fprintf (stderr, "\n");
    va_end (args);
}

static char *
FcConfigContent (xmlDocPtr    doc,
		 xmlNodePtr   node)
{
    char	    *content;
    
    content = xmlNodeListGetString (doc, node->children, 1);
    if (!content)
    {
	FcConfigParseError ("<%s> must have content",
			    node->name);
	return FcFalse;
    }
    return content;
}

static char *
FcConfigAttr (xmlDocPtr	    doc,
	      xmlAttrPtr    attr)
{
    char	    *content;
    
    content = xmlNodeListGetString (doc, attr->children, 1);
    if (!content)
    {
	FcConfigParseError ("attribute %s must have a value",
			    attr->name);
	return FcFalse;
    }
    return content;
}

static struct {
    char    *name;
    FcOp    op;
} fcOps[] = {
    { "int",		FcOpInteger	    },
    { "double",		FcOpDouble	    },
    { "string",		FcOpString	    },
    { "matrix",		FcOpMatrix	    },
    { "bool",		FcOpBool	    },
    { "charset",	FcOpCharSet	    },
    { "name",		FcOpField	    },
    { "const",		FcOpConst	    },
    { "field",		FcOpField	    },
    { "if",		FcOpQuest	    },
    { "or",		FcOpOr		    },
    { "and",		FcOpAnd		    },
    { "eq",		FcOpEqual	    },
    { "not_eq",		FcOpNotEqual	    },
    { "less",		FcOpLess	    },
    { "less_eq",	FcOpLessEqual	    },
    { "more",		FcOpMore	    },
    { "more_eq",	FcOpMoreEqual	    },
    { "plus",		FcOpPlus	    },
    { "minus",		FcOpMinus	    },
    { "times",		FcOpTimes	    },
    { "divide",		FcOpDivide	    },
    { "not",		FcOpNot		    },
    { "assign",		FcOpAssign	    },
    { "assign_replace",	FcOpAssignReplace   },
    { "prepend",	FcOpPrepend	    },
    { "prepend_first",	FcOpPrependFirst    },
    { "append",		FcOpAppend	    },
    { "append_last",	FcOpAppendLast	    },
};

#define NUM_OPS (sizeof fcOps / sizeof fcOps[0])

static FcOp
FcConfigLexOp (const char *op)
{
    int	i;

    for (i = 0; i < NUM_OPS; i++)
	if (!strcmp (op, fcOps[i].name)) return fcOps[i].op;
    return FcOpInvalid;
}

static FcBool
FcConfigLexBool (const char *bool)
{
    if (*bool == 't' || *bool == 'T')
	return FcTrue;
    if (*bool == 'y' || *bool == 'Y')
	return FcTrue;
    if (*bool == '1')
	return FcTrue;
    return FcFalse;
}

static FcBool
FcConfigParseDir (FcConfig	*config,
		  xmlDocPtr	doc,
		  xmlNodePtr	dir)
{
    char    *content = FcConfigContent (doc, dir);

    if (!content)
	return FcFalse;
    return FcConfigAddDir (config, content);
}

static FcBool
FcConfigParseCache (FcConfig	*config,
		    xmlDocPtr	doc,
		    xmlNodePtr	dir)
{
    char    *content = FcConfigContent (doc, dir);

    if (!content)
	return FcFalse;
    return FcConfigSetCache (config, content);
}

static FcBool
FcConfigParseInclude (FcConfig	    *config,
		      xmlDocPtr	    doc,
		      xmlNodePtr    inc)
{
    char	*content = FcConfigContent (doc, inc);
    xmlAttr	*attr;
    FcBool	complain = FcTrue;

    if (!content)
	return FcFalse;
    
    for (attr = inc->properties; attr; attr = attr->next)
    {
	if (attr->type != XML_ATTRIBUTE_NODE)
	    continue;
	if (!strcmp (attr->name, "ignore_missing"))
	    complain = !FcConfigLexBool (FcConfigAttr (doc, attr));
    }
    return FcConfigParseAndLoad (config, content, complain);
}

static FcBool
FcConfigParseBlank (FcConfig	    *config,
		    xmlDocPtr	    doc,
		    xmlNodePtr	    blank)
{
    xmlNode	*node;
    FcChar32	ucs4;

    for (node = blank->children; node; node = node->next)
    {
	if (node->type != XML_ELEMENT_NODE)
	    continue;
	if (!strcmp (node->name, "int"))
	{
	    ucs4 = (FcChar32) strtol (FcConfigContent (doc, node), 0, 0);
	    if (!config->blanks)
	    {
		config->blanks = FcBlanksCreate ();
		if (!config->blanks)
		    break;
	    }
	    if (!FcBlanksAdd (config->blanks, ucs4))
		break;
	}
    }
    if (node)
	return FcFalse;
    return FcTrue;
}

static FcBool
FcConfigParseConfig (FcConfig	    *config,
		     xmlDocPtr	    doc,
		     xmlNodePtr	    cfg)
{
    xmlNode	*node;

    for (node = cfg->children; node; node = node->next)
    {
	if (node->type != XML_ELEMENT_NODE)
	    continue;
	if (!strcmp (node->name, "blank"))
	{
	    if (!FcConfigParseBlank (config, doc, node))
		break;
	}
    }
    if (node)
	return FcFalse;
    return FcTrue;
}

static FcMatrix *
FcConfigParseMatrix (xmlDocPtr	doc,
		     xmlNodePtr	node)
{
    static FcMatrix m;
    enum { m_xx, m_xy, m_yx, m_yy, m_done } matrix_state = m_xx;
    double  v;
    char    *text;
    
    FcMatrixInit (&m);

    for (; node; node = node->next)
    {
	if (node->type != XML_ELEMENT_NODE)
	    continue;
	if (strcmp (node->name, "double"))
	    continue;
	text = FcConfigContent (doc, node);
	if (!text)
	    continue;
	v = strtod (text, 0);
	switch (matrix_state) {
	case m_xx: m.xx = v; break;
	case m_xy: m.xy = v; break;
	case m_yx: m.yx = v; break;
	case m_yy: m.yy = v; break;
	default: break;
	}
	matrix_state++;
    }
	 
    return &m;
}

static FcExpr *
FcConfigParseExpr (xmlDocPtr	doc,
		   xmlNodePtr	expr)
{
    FcOp	op = FcConfigLexOp (expr->name);
    xmlNodePtr	node;
    FcExpr	*l = 0, *e = 0, *r = 0, *c = 0;

    switch (op) {
    case FcOpInteger:
	l = FcExprCreateInteger (strtol (FcConfigContent (doc, expr), 0, 0));
	break;
    case FcOpDouble:
	l = FcExprCreateDouble (strtod (FcConfigContent (doc, expr), 0));
	break;
    case FcOpString:
	l = FcExprCreateString (FcConfigContent (doc, expr));
	break;
    case FcOpMatrix:
	l = FcExprCreateMatrix (FcConfigParseMatrix (doc, expr));
	break;
    case FcOpBool:
	l = FcExprCreateBool (FcConfigLexBool(FcConfigContent (doc, expr)));
	break;
    case FcOpCharSet:
	/* not sure what to do here yet */
	break;
    case FcOpField:
	l = FcExprCreateField (FcConfigContent (doc, expr));
	break;
    case FcOpConst:
	l = FcExprCreateConst (FcConfigContent (doc, expr));
	break;
    case FcOpQuest:
	for (node = expr->children; node; node = node->next)
	{
	    if (node->type != XML_ELEMENT_NODE)
		continue;
	    e = FcConfigParseExpr (doc, node);
	    if (!e)
		break;
	    if (!l)
		l = e;
	    else if (!c)
		c = e;
	    else if (!r)
		r = e;
	    else
		FcExprDestroy (e);
	}
	e = 0;
	if (!node && l && c && r)
	{
	    e = FcExprCreateOp (c, FcOpQuest, r);
	    if (e)
	    {
		r = e;
		c = 0;
		e = FcExprCreateOp (l, FcOpQuest, r);
	    }
	    if (!e)
		node = expr->children;
	}
	if (node || !l || !c || !r || !e)
	{
	    if (l)
		FcExprDestroy (l);
	    if (c)
		FcExprDestroy (c);
	    if (r)
		FcExprDestroy (r);
	    return 0;
	}
	break;
    default:
	for (node = expr->children; node; node = node->next)
	{
	    if (node->type != XML_ELEMENT_NODE)
		continue;
	    e = FcConfigParseExpr (doc, node);
	    if (!e)
		break;
	    if (!l)
		l = e;
	    else
	    {
		r = e;
		e = FcExprCreateOp (l, op, r);
		if (!e)
		{
		    FcExprDestroy (r);
		    break;
		}
		l = e;
	    }
	}
	if (node || !l)
	{
	    if (l)
		FcExprDestroy (l);
	    return 0;
	}
	/*
	 * Special case for unary ops 
	 */
	if (!r)
	{
	    e = FcExprCreateOp (l, op, 0);
	    if (!e)
	    {
		FcExprDestroy (l);
		return 0;
	    }
	}
	break;
    
    case FcOpInvalid:
	return 0;
    }
    return l;
}

static FcTest *
FcConfigParseTest (xmlDocPtr	doc,
		   xmlNodePtr	test)
{
    xmlNodePtr	node;
    xmlAttrPtr	attr;
    FcQual	qual = FcQualAny;
    FcOp	op = FcOpEqual;
    char	*field = 0;
    FcExpr	*expr = 0;

    for (attr = test->properties; attr; attr = attr->next)
    {
	if (attr->type != XML_ATTRIBUTE_NODE)
	    continue;
	if (!strcmp (attr->name, "qual"))
	{
	    char    *qual_name = FcConfigAttr (doc, attr);
	    if (!qual_name)
		;
	    else if (!strcmp (qual_name, "any"))
		qual = FcQualAny;
	    else if (!strcmp (qual_name, "all"))
		qual = FcQualAll;
	}
	else if (!strcmp (attr->name, "name"))
	{
	    field = FcConfigAttr (doc, attr);
	}
	else if (!strcmp (attr->name, "compare"))
	{
	    char    *compare = FcConfigAttr (doc, attr);
	    
	    if (!compare || (op = FcConfigLexOp (compare)) == FcOpInvalid)
	    {
		FcConfigParseError ("Invalid comparison %s", 
				    compare ? compare : "<missing>");
		return 0;
	    }
	}
    }
    if (attr)
	return 0;

    for (node = test->children; node; node = node->next)
    {
	if (node->type != XML_ELEMENT_NODE)
	    continue;
	expr = FcConfigParseExpr (doc, node);
	if (!expr)
	    return 0;
	break;
    }

    if (!expr)
    {
	FcConfigParseError ("Missing test expression");
	return 0;
    }
    
    return FcTestCreate (qual, field, op, expr);
}

static FcExpr *
FcConfigParseExprList (xmlDocPtr    doc,
		       xmlNodePtr   expr)
{
    FcExpr  *l, *e, *r;
    
    if (!expr)
	return 0;
    
    e = FcConfigParseExprList (doc, expr->next);

    if (expr->type == XML_ELEMENT_NODE)
    {
	r = e;
	l = FcConfigParseExpr (doc, expr);
	if (!l)
	    goto bail;
	if (r)
	{
	    e = FcExprCreateOp (l, FcOpComma, r);
	    if (!e)
		goto bail;
	}
	else
	    e = l;
    }
    
    return e;
bail:
    if (l)
	FcExprDestroy (l);
    if (r)
	FcExprDestroy (r);
    return 0;
}

static FcEdit *
FcConfigParseEdit (xmlDocPtr	doc,
		   xmlNodePtr	edit)
{
    xmlAttrPtr	attr;
    char	*name = 0;
    FcOp	mode = FcOpAssign;
    FcExpr	*e;
    FcEdit	*ed;

    for (attr = edit->properties; attr; attr = attr->next)
    {
	if (attr->type != XML_ATTRIBUTE_NODE)
	    continue;
	if (!strcmp (attr->name, "name"))
	    name = FcConfigAttr (doc, attr);
	else if (!strcmp (attr->name, "mode"))
	    mode = FcConfigLexOp (FcConfigAttr (doc, attr));
    }

    e = FcConfigParseExprList (doc, edit->children);

    ed = FcEditCreate (name, mode, e);
    if (!ed)
	FcExprDestroy (e);
    return ed;
}

static FcBool
FcConfigParseMatch (FcConfig	*config,
		    xmlDocPtr	doc,
		    xmlNodePtr	match)
{
    xmlNodePtr	node;
    xmlAttrPtr	attr;
    FcTest	*tests = 0, **prevTest = &tests, *test;
    FcEdit	*edits = 0, **prevEdit = &edits, *edit;
    FcMatchKind	kind;
    FcBool	found_kind = FcFalse;

    for (node = match->children; node; node = node->next)
    {
	if (node->type != XML_ELEMENT_NODE)
	    continue;
	if (!strcmp (node->name, "test"))
	{
	    test = FcConfigParseTest (doc, node);
	    if (!test)
		break;
	    *prevTest = test;
	    prevTest = &test->next;
	}
	else if (!strcmp (node->name, "edit"))
	{
	    edit = FcConfigParseEdit (doc, node);
	    if (!edit)
		break;
	    *prevEdit = edit;
	    prevEdit = &edit->next;
	}
    }

    for (attr = match->properties; attr; attr = attr->next)
    {
	if (attr->type != XML_ATTRIBUTE_NODE)
	    continue;
	if (!strcmp (attr->name, "target"))
	{
	    char    *target = FcConfigAttr (doc, attr);
	    if (!target)
	    {
		FcConfigParseError ("Missing match target");
		break;
	    }
	    else if (!strcmp (target, "pattern"))
	    {
		kind = FcMatchPattern;
		found_kind = FcTrue;
	    }
	    else if (!strcmp (target, "font"))
	    {
		kind = FcMatchFont;
		found_kind = FcTrue;
	    }
	}
    }

    if (node || attr || !found_kind || 
	!FcConfigAddEdit (config, tests, edits, kind))
    {
	if (tests)
	    FcTestDestroy (tests);
	if (edits)
	    FcEditDestroy (edits);
	return FcFalse;
    }

    return FcTrue;
}

static FcExpr *
FcConfigParseFamilies (xmlDocPtr    doc,
		       xmlNodePtr   family)
{
    FcExpr  *next = 0, *this = 0, *expr = 0;
    
    if (!family)
	return 0;
    next = FcConfigParseFamilies (doc, family->next);
    
    if (family->type == XML_ELEMENT_NODE && !strcmp (family->name, "family"))
    {
	this = FcExprCreateString (FcConfigContent (doc, family));
	if (!this)
	    goto bail;
	if (next)
	{
	    expr = FcExprCreateOp (this, FcOpComma, next);
	    if (!expr)
		goto bail;
	}
	else
	    expr = this;
    }
    else
	expr = next;
    return expr;

bail:
    if (expr)
	FcExprDestroy (expr);
    if (this)
	FcExprDestroy (this);
    if (next)
	FcExprDestroy (next);
    return 0;
}

static FcBool
FcConfigParseAlias (FcConfig	*config,
		    xmlDocPtr	doc,
		    xmlNodePtr	alias)
{
    xmlNodePtr	node;
    FcExpr	*prefer = 0, *accept = 0, *def = 0;
    FcExpr	*family;
    FcEdit	*edit = 0, *next;
    FcTest	*test;

    for (node = alias->children; node; node = node->next)
    {
	if (node->type != XML_ELEMENT_NODE)
	    continue;
	if (!strcmp (node->name, "family"))
	    family = FcExprCreateString (FcConfigContent (doc, node));
	else if (!strcmp (node->name, "prefer"))
	    prefer = FcConfigParseFamilies (doc, node->children);
	else if (!strcmp (node->name, "accept"))
	    accept = FcConfigParseFamilies (doc, node->children);
	else if (!strcmp (node->name, "default"))
	    def = FcConfigParseFamilies (doc, node->children);
    }
    
    if (prefer)
    {
	edit = FcEditCreate (FcConfigSaveField ("family"),
			     FcOpPrepend,
			     prefer);
	if (edit)
	    edit->next = 0;
    }
    if (accept)
    {
	next = edit;
	edit = FcEditCreate (FcConfigSaveField ("family"),
			     FcOpAppend,
			     accept);
	if (edit)
	    edit->next = next;
    }
    if (def)
    {
	next = edit;
	edit = FcEditCreate (FcConfigSaveField ("family"),
			      FcOpAppendLast,
			      def);
	if (edit)
	    edit->next = next;
    }
    if (edit)
    {
	test = FcTestCreate (FcQualAny,
			     FcConfigSaveField ("family"),
			     FcOpEqual,
			     family);
	if (test)
	    FcConfigAddEdit (config, test, edit, FcMatchPattern);
    }
    return FcTrue;
}

FcBool
FcConfigParse (FcConfig	    *config,
	       xmlDocPtr    doc)
{
    xmlNodePtr	cur;
    xmlNodePtr	node;
    
    cur = xmlDocGetRootElement (doc);

    for (node = cur->children; node; node = node->next)
    {
	if (node->type != XML_ELEMENT_NODE)
	    continue;
	if (!strcmp (node->name, "dir"))
	{
	    if (!FcConfigParseDir (config, doc, node))
		break;
	}
	else if (!strcmp (node->name, "cache"))
	{
	    if (!FcConfigParseCache (config, doc, node))
		break;
	}
	else if (!strcmp (node->name, "include"))
	{
	    if (!FcConfigParseInclude (config, doc, node))
		break;
	}
	else if (!strcmp (node->name, "config"))
	{
	    if (!FcConfigParseConfig (config, doc, node))
		break;
	}
	else if (!strcmp (node->name, "match"))
	{
	    if (!FcConfigParseMatch (config, doc, node))
		break;
	}
	else if (!strcmp (node->name, "alias"))
	{
	    if (!FcConfigParseAlias (config, doc, node))
		break;
	}
	else
	{
	    FcConfigParseError ("invalid element %s", node->name);
	    break;
	}   
    }
    if (node)
	return FcFalse;
    return FcTrue;
}

FcBool
FcConfigParseAndLoad (FcConfig	    *config,
		      const char    *file,
		      FcBool	    complain)
{
    xmlDocPtr	doc;
    FcBool	ret;

    doc = FcConfigLoad (file);
    if (doc)
    {
	ret = FcConfigAddConfigFile (config, file);
	if (ret)
	    ret = FcConfigParse (config, doc);
	xmlFreeDoc (doc);
	return ret;
    }
    if (complain)
    {
	if (file)
	    FcConfigParseError ("Cannot load config file \"%s\"", file);
	else
	    FcConfigParseError ("Cannot load default config file");
	return FcFalse;
    }
    return FcTrue;
}
