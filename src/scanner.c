/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <expat.h>

#include "wayland-util.h"

static int
usage(int ret)
{
	fprintf(stderr, "usage: ./scanner [client-header|server-header|code]\n");
	exit(ret);
}

#define XML_BUFFER_SIZE 4096

struct description {
	char *summary;
	char *text;
};

struct protocol {
	char *name;
	char *uppercase_name;
	struct wl_list interface_list;
	int type_index;
	int null_run_length;
	char *copyright;
	struct description *description;
};

struct interface {
	char *name;
	char *uppercase_name;
	int version;
	int since;
	struct wl_list request_list;
	struct wl_list event_list;
	struct wl_list enumeration_list;
	struct wl_list link;
	struct description *description;
};

struct message {
	char *name;
	char *uppercase_name;
	struct wl_list arg_list;
	struct wl_list link;
	int arg_count;
	int type_index;
	int all_null;
	int destructor;
	int since;
	struct description *description;
};

enum arg_type {
	NEW_ID,
	INT,
	UNSIGNED,
	STRING,
	OBJECT,
	ARRAY,
	FD
};

struct arg {
	char *name;
	enum arg_type type;
	char *interface_name;
	struct wl_list link;
	char *summary;
};

struct enumeration {
	char *name;
	char *uppercase_name;
	struct wl_list entry_list;
	struct wl_list link;
	struct description *description;
};

struct entry {
	char *name;
	char *uppercase_name;
	char *value;
	char *summary;
	struct wl_list link;
};

struct parse_context {
	const char *filename;
	XML_Parser parser;
	struct protocol *protocol;
	struct interface *interface;
	struct message *message;
	struct enumeration *enumeration;
	struct description *description;
	char character_data[8192];
	unsigned int character_data_length;
};

static char *
uppercase_dup(const char *src)
{
	char *u;
	int i;

	u = strdup(src);
	for (i = 0; u[i]; i++)
		u[i] = toupper(u[i]);
	u[i] = '\0';

	return u;
}

static const char *indent(int n)
{
	const char *whitespace[] = {
		"\t\t\t\t\t\t\t\t\t\t\t\t",
		"\t\t\t\t\t\t\t\t\t\t\t\t ",
		"\t\t\t\t\t\t\t\t\t\t\t\t  ",
		"\t\t\t\t\t\t\t\t\t\t\t\t   ",
		"\t\t\t\t\t\t\t\t\t\t\t\t    ",
		"\t\t\t\t\t\t\t\t\t\t\t\t     ",
		"\t\t\t\t\t\t\t\t\t\t\t\t      ",
		"\t\t\t\t\t\t\t\t\t\t\t\t       "
	};

	return whitespace[n % 8] + 12 - n / 8;
}

static void
desc_dump(char *src, int startcol)
{
	int i, j, col = startcol;

	for (i = 0; src[i]; i++) {
		if (isspace(src[i]))
		    continue;

		j = i;
		while (src[i] && !isspace(src[i]))
			i++;

		if (col + i - j > 72) {
			printf("\n%s * ", indent(startcol));
			col = startcol;
		}

		col += printf("%s%.*s",
			      col > startcol ? " " : "", i - j, &src[j]);
	}
}

static void
fail(struct parse_context *ctx, const char *msg)
{
	fprintf(stderr, "%s:%ld: %s\n",
		ctx->filename, XML_GetCurrentLineNumber(ctx->parser), msg);
	exit(EXIT_FAILURE);
}

static void
start_element(void *data, const char *element_name, const char **atts)
{
	struct parse_context *ctx = data;
	struct interface *interface;
	struct message *message;
	struct arg *arg;
	struct enumeration *enumeration;
	struct entry *entry;
	struct description *description;
	const char *name, *type, *interface_name, *value, *summary, *since;
	char *end;
	int i, version;

	name = NULL;
	type = NULL;
	version = 0;
	interface_name = NULL;
	value = NULL;
	summary = NULL;
	description = NULL;
	since = NULL;
	for (i = 0; atts[i]; i += 2) {
		if (strcmp(atts[i], "name") == 0)
			name = atts[i + 1];
		if (strcmp(atts[i], "version") == 0)
			version = atoi(atts[i + 1]);
		if (strcmp(atts[i], "type") == 0)
			type = atts[i + 1];
		if (strcmp(atts[i], "value") == 0)
			value = atts[i + 1];
		if (strcmp(atts[i], "interface") == 0)
			interface_name = atts[i + 1];
		if (strcmp(atts[i], "summary") == 0)
			summary = atts[i + 1];
		if (strcmp(atts[i], "since") == 0)
			since = atts[i + 1];
	}

	ctx->character_data_length = 0;
	if (strcmp(element_name, "protocol") == 0) {
		if (name == NULL)
			fail(ctx, "no protocol name given");

		ctx->protocol->name = strdup(name);
		ctx->protocol->uppercase_name = uppercase_dup(name);
		ctx->protocol->description = NULL;
	} else if (strcmp(element_name, "copyright") == 0) {
		
	} else if (strcmp(element_name, "interface") == 0) {
		if (name == NULL)
			fail(ctx, "no interface name given");

		if (version == 0)
			fail(ctx, "no interface version given");

		interface = malloc(sizeof *interface);
		interface->name = strdup(name);
		interface->uppercase_name = uppercase_dup(name);
		interface->version = version;
		interface->description = NULL;
		interface->since = 1;
		wl_list_init(&interface->request_list);
		wl_list_init(&interface->event_list);
		wl_list_init(&interface->enumeration_list);
		wl_list_insert(ctx->protocol->interface_list.prev,
			       &interface->link);
		ctx->interface = interface;
	} else if (strcmp(element_name, "request") == 0 ||
		   strcmp(element_name, "event") == 0) {
		if (name == NULL)
			fail(ctx, "no request name given");

		message = malloc(sizeof *message);
		message->name = strdup(name);
		message->uppercase_name = uppercase_dup(name);
		wl_list_init(&message->arg_list);
		message->arg_count = 0;
		message->description = NULL;

		if (strcmp(element_name, "request") == 0)
			wl_list_insert(ctx->interface->request_list.prev,
				       &message->link);
		else
			wl_list_insert(ctx->interface->event_list.prev,
				       &message->link);

		if (type != NULL && strcmp(type, "destructor") == 0)
			message->destructor = 1;
		else
			message->destructor = 0;

		if (since != NULL) {
			version = strtol(since, &end, 0);
			if (errno == EINVAL || end == since || *end != '\0')
				fail(ctx, "invalid integer\n");
			if (version <= ctx->interface->since)
				fail(ctx, "since version not increasing\n");
			ctx->interface->since = version;
		}

		message->since = ctx->interface->since;

		if (strcmp(name, "destroy") == 0 && !message->destructor)
			fail(ctx, "destroy request should be destructor type");

		ctx->message = message;
	} else if (strcmp(element_name, "arg") == 0) {
		arg = malloc(sizeof *arg);
		arg->name = strdup(name);

		if (strcmp(type, "int") == 0)
			arg->type = INT;
		else if (strcmp(type, "uint") == 0)
			arg->type = UNSIGNED;
		else if (strcmp(type, "string") == 0)
			arg->type = STRING;
		else if (strcmp(type, "array") == 0)
			arg->type = ARRAY;
		else if (strcmp(type, "fd") == 0)
			arg->type = FD;
		else if (strcmp(type, "new_id") == 0) {
			arg->type = NEW_ID;
		} else if (strcmp(type, "object") == 0) {
			arg->type = OBJECT;
		} else {
			fail(ctx, "unknown type");
		}

		switch (arg->type) {
		case NEW_ID:
		case OBJECT:
			if (interface_name == NULL)
				fail(ctx, "no interface name given");
			arg->interface_name = strdup(interface_name);
			break;
		default:
			if (interface_name != NULL)
				fail(ctx, "interface not allowed");
			break;
		}

		arg->summary = NULL;
		if (summary)
			arg->summary = strdup(summary);

		wl_list_insert(ctx->message->arg_list.prev, &arg->link);
		ctx->message->arg_count++;
	} else if (strcmp(element_name, "enum") == 0) {
		if (name == NULL)
			fail(ctx, "no enum name given");

		enumeration = malloc(sizeof *enumeration);
		enumeration->name = strdup(name);
		enumeration->uppercase_name = uppercase_dup(name);
		enumeration->description = NULL;
		wl_list_init(&enumeration->entry_list);

		wl_list_insert(ctx->interface->enumeration_list.prev,
			       &enumeration->link);

		ctx->enumeration = enumeration;
	} else if (strcmp(element_name, "entry") == 0) {
		if (name == NULL)
			fail(ctx, "no entry name given");

		entry = malloc(sizeof *entry);
		entry->name = strdup(name);
		entry->uppercase_name = uppercase_dup(name);
		entry->value = strdup(value);
		if (summary)
			entry->summary = strdup(summary);
		else
			entry->summary = NULL;
		wl_list_insert(ctx->enumeration->entry_list.prev,
			       &entry->link);
	} else if (strcmp(element_name, "description") == 0) {
		if (summary == NULL)
			fail(ctx, "description without summary");

		description = malloc(sizeof *description);
		if (summary)
			description->summary = strdup(summary);
		else
			description->summary = NULL;

		if (ctx->message)
			ctx->message->description = description;
		else if (ctx->enumeration)
			ctx->enumeration->description = description;
		else if (ctx->interface)
			ctx->interface->description = description;
		else
			ctx->protocol->description = description;
		ctx->description = description;
	}
}

static void
end_element(void *data, const XML_Char *name)
{
	struct parse_context *ctx = data;

	if (strcmp(name, "copyright") == 0) {
		ctx->protocol->copyright =
			strndup(ctx->character_data,
				ctx->character_data_length);
	} else if (strcmp(name, "description") == 0) {
		char *text = strndup(ctx->character_data,
				     ctx->character_data_length);
		if (text)
			ctx->description->text = strdup(text);
		ctx->description = NULL;
	} else if (strcmp(name, "request") == 0 ||
		   strcmp(name, "event") == 0) {
		ctx->message = NULL;
	} else if (strcmp(name, "enum") == 0) {
		ctx->enumeration = NULL;
	}
}

static void
character_data(void *data, const XML_Char *s, int len)
{
	struct parse_context *ctx = data;

	if (ctx->character_data_length + len > sizeof (ctx->character_data)) {
		fprintf(stderr, "too much character data");
		exit(EXIT_FAILURE);
	    }

	memcpy(ctx->character_data + ctx->character_data_length, s, len);
	ctx->character_data_length += len;
}

static void
emit_opcodes(struct wl_list *message_list, struct interface *interface)
{
	struct message *m;
	int opcode;

	if (wl_list_empty(message_list))
		return;

	opcode = 0;
	wl_list_for_each(m, message_list, link)
		printf("#define %s_%s\t%d\n",
		       interface->uppercase_name, m->uppercase_name, opcode++);

	printf("\n");
}

static void
emit_type(struct arg *a)
{
	switch (a->type) {
	default:
	case INT:
	case FD:
		printf("int32_t ");
		break;
	case NEW_ID:
	case UNSIGNED:
		printf("uint32_t ");
		break;
	case STRING:
		printf("const char *");
		break;
	case OBJECT:
		printf("struct %s *", a->interface_name);
		break;
	case ARRAY:
		printf("struct wl_array *");
		break;
	}
}

static void
emit_stubs(struct wl_list *message_list, struct interface *interface)
{
	struct message *m;
	struct arg *a, *ret;
	int has_destructor, has_destroy;

	/* We provide a hand written functions for the display object */
	if (strcmp(interface->name, "wl_display") == 0)
		return;

	printf("static inline void\n"
	       "%s_set_user_data(struct %s *%s, void *user_data)\n"
	       "{\n"
	       "\twl_proxy_set_user_data((struct wl_proxy *) %s, user_data);\n"
	       "}\n\n",
	       interface->name, interface->name, interface->name,
	       interface->name);

	printf("static inline void *\n"
	       "%s_get_user_data(struct %s *%s)\n"
	       "{\n"
	       "\treturn wl_proxy_get_user_data((struct wl_proxy *) %s);\n"
	       "}\n\n",
	       interface->name, interface->name, interface->name,
	       interface->name);

	has_destructor = 0;
	has_destroy = 0;
	wl_list_for_each(m, message_list, link) {
		if (m->destructor)
			has_destructor = 1;
		if (strcmp(m->name, "destroy)") == 0)
			has_destroy = 1;
	}

	if (!has_destructor && has_destroy) {
		fprintf(stderr,
			"interface %s has method named destroy but"
			"no destructor", interface->name);
		exit(EXIT_FAILURE);
	}

	if (!has_destructor)
		printf("static inline void\n"
		       "%s_destroy(struct %s *%s)\n"
		       "{\n"
		       "\twl_proxy_destroy("
		       "(struct wl_proxy *) %s);\n"
		       "}\n\n",
		       interface->name, interface->name, interface->name,
		       interface->name);

	if (wl_list_empty(message_list))
		return;

	wl_list_for_each(m, message_list, link) {
		ret = NULL;
		wl_list_for_each(a, &m->arg_list, link) {
			if (a->type == NEW_ID)
				ret = a;
		}

		if (ret)
			printf("static inline struct %s *\n",
			       ret->interface_name);
		else
			printf("static inline void\n");

		printf("%s_%s(struct %s *%s",
		       interface->name, m->name,
		       interface->name, interface->name);

		wl_list_for_each(a, &m->arg_list, link) {
			if (a->type == NEW_ID)
				continue;
			printf(", ");
			emit_type(a);
			printf("%s", a->name);
		}

		printf(")\n"
		       "{\n");
		if (ret)
			printf("\tstruct wl_proxy *%s;\n\n"
			       "\t%s = wl_proxy_create("
			       "(struct wl_proxy *) %s,\n"
			       "\t\t\t     &%s_interface);\n"
			       "\tif (!%s)\n"
			       "\t\treturn NULL;\n\n",
			       ret->name,
			       ret->name,
			       interface->name, ret->interface_name,
			       ret->name);

		printf("\twl_proxy_marshal((struct wl_proxy *) %s,\n"
		       "\t\t\t %s_%s",
		       interface->name,
		       interface->uppercase_name,
		       m->uppercase_name);

		wl_list_for_each(a, &m->arg_list, link) {
			printf(", ");
				printf("%s", a->name);
		}
		printf(");\n");

		if (m->destructor)
			printf("\n\twl_proxy_destroy("
			       "(struct wl_proxy *) %s);\n",
			       interface->name);

		if (ret)
			printf("\n\treturn (struct %s *) %s;\n",
			       ret->interface_name, ret->name);

		printf("}\n\n");
	}
}

static void
emit_event_wrappers(struct wl_list *message_list, struct interface *interface)
{
	struct message *m;
	struct arg *a;

	/* We provide hand written functions for the display object */
	if (strcmp(interface->name, "wl_display") == 0)
		return;

	wl_list_for_each(m, message_list, link) {
		printf("static inline void\n"
		       "%s_send_%s(struct wl_resource *resource_",
		       interface->name, m->name);

		wl_list_for_each(a, &m->arg_list, link) {
			printf(", ");
			switch (a->type) {
			case NEW_ID:
			case OBJECT:
				printf("struct wl_resource *");
				break;
			default:
				emit_type(a);
			}
			printf("%s", a->name);
		}

		printf(")\n"
		       "{\n"
		       "\twl_resource_post_event(resource_, %s_%s",
		       interface->uppercase_name, m->uppercase_name);

		wl_list_for_each(a, &m->arg_list, link)
			printf(", %s", a->name);

		printf(");\n");
		printf("}\n\n");
	}
}

static void
emit_enumerations(struct interface *interface)
{
	struct enumeration *e;
	struct entry *entry;

	wl_list_for_each(e, &interface->enumeration_list, link) {
		struct description *desc = e->description;

		printf("#ifndef %s_%s_ENUM\n",
		       interface->uppercase_name, e->uppercase_name);
		printf("#define %s_%s_ENUM\n",
		       interface->uppercase_name, e->uppercase_name);

		if (desc) {
			printf("/**\n"
			       " * %s_%s - %s\n", interface->name,
			       e->name, desc->summary);
			wl_list_for_each(entry, &e->entry_list, link) {
				printf(" * @%s_%s_%s: %s\n",
				       interface->uppercase_name,
				       e->uppercase_name,
				       entry->uppercase_name,
				       entry->summary);
			}
			if (desc->text) {
				printf(" *\n"
				       " * ");
				desc_dump(desc->text, 0);
			}
			printf(" */\n");
		}
		printf("enum %s_%s {\n", interface->name, e->name);
		wl_list_for_each(entry, &e->entry_list, link)
			printf("\t%s_%s_%s = %s,\n",
			       interface->uppercase_name,
			       e->uppercase_name,
			       entry->uppercase_name, entry->value);
		printf("};\n");
		printf("#endif /* %s_%s_ENUM */\n\n",
		       interface->uppercase_name, e->uppercase_name);
	}
}

static void
emit_structs(struct wl_list *message_list, struct interface *interface)
{
	struct message *m;
	struct arg *a;
	int is_interface, n;

	if (wl_list_empty(message_list))
		return;

	is_interface = message_list == &interface->request_list;
	if (interface->description) {
		struct description *desc = interface->description;
		printf("/**\n"
		       " * %s - %s\n", interface->name, desc->summary);
		wl_list_for_each(m, message_list, link) {
			struct description *mdesc = m->description;
			printf(" * @%s: %s\n", m->name, mdesc ? mdesc->summary :
			       "(none)");
		}
		printf(" *\n"
		       " * ");
		desc_dump(desc->text, 0);
		printf(" */\n");
	}
	printf("struct %s_%s {\n", interface->name,
	       is_interface ? "interface" : "listener");

	wl_list_for_each(m, message_list, link) {
		struct description *mdesc = m->description;

		printf("\t/**\n");
		printf("\t * %s - %s\n", m->name, mdesc ? mdesc->summary :
		       "(none)");
		wl_list_for_each(a, &m->arg_list, link) {
			printf("\t * @%s: %s\n", a->name, a->summary ?
			       a->summary : "(none)");
		}
		if (mdesc) {
			printf("\t * ");
			desc_dump(mdesc->text, 8);
			printf("\n");
		}
		if (m->since > 1) {
			printf("\t * @since: %d\n", m->since);
		}
		printf("\t */\n");
		printf("\tvoid (*%s)(", m->name);

		n = strlen(m->name) + 17;
		if (is_interface) {
			printf("struct wl_client *client,\n"
			       "%sstruct wl_resource *resource",
			       indent(n));
		} else {
			printf("void *data,\n"),
			printf("%sstruct %s *%s",
			       indent(n), interface->name, interface->name);
		}

		wl_list_for_each(a, &m->arg_list, link) {
			printf(",\n%s", indent(n));

			if (is_interface && a->type == OBJECT)
				printf("struct wl_resource *");
			else
				emit_type(a);

			printf("%s", a->name);
		}

		printf(");\n");
	}

	printf("};\n\n");

	if (!is_interface) {
	    printf("static inline int\n"
		   "%s_add_listener(struct %s *%s,\n"
		   "%sconst struct %s_listener *listener, void *data)\n"
		   "{\n"
		   "\treturn wl_proxy_add_listener((struct wl_proxy *) %s,\n"
		   "%s(void (**)(void)) listener, data);\n"
		   "}\n\n",
		   interface->name, interface->name, interface->name,
		   indent(14 + strlen(interface->name)),
		   interface->name,
		   interface->name,
		   indent(37));
	}
}

static void
format_copyright(const char *copyright)
{
	int bol = 1, start = 0, i;

	for (i = 0; copyright[i]; i++) {
		if (bol && (copyright[i] == ' ' || copyright[i] == '\t')) {
			continue;
		} else if (bol) {
			bol = 0;
			start = i;
		}

		if (copyright[i] == '\n' || copyright[i] == '\0') {
			printf("%s %.*s\n",
			       i == 0 ? "/*" : " *",
			       i - start, copyright + start);
			bol = 1;
		}
	}
	printf(" */\n\n");
}

static void
emit_header(struct protocol *protocol, int server)
{
	struct interface *i;
	const char *s = server ? "SERVER" : "CLIENT";

	if (protocol->copyright)
		format_copyright(protocol->copyright);

	printf("#ifndef %s_%s_PROTOCOL_H\n"
	       "#define %s_%s_PROTOCOL_H\n"
	       "\n"
	       "#ifdef  __cplusplus\n"
	       "extern \"C\" {\n"
	       "#endif\n"
	       "\n"
	       "#include <stdint.h>\n"
	       "#include <stddef.h>\n"
	       "#include \"wayland-util.h\"\n\n"
	       "struct wl_client;\n"
	       "struct wl_resource;\n\n",
	       protocol->uppercase_name, s,
	       protocol->uppercase_name, s);

	wl_list_for_each(i, &protocol->interface_list, link)
		printf("struct %s;\n", i->name);
	printf("\n");

	wl_list_for_each(i, &protocol->interface_list, link) {
		printf("extern const struct wl_interface "
		       "%s_interface;\n",
		       i->name);
	}
	printf("\n");

	wl_list_for_each(i, &protocol->interface_list, link) {

		emit_enumerations(i);

		if (server) {
			emit_structs(&i->request_list, i);
			emit_opcodes(&i->event_list, i);
			emit_event_wrappers(&i->event_list, i);
		} else {
			emit_structs(&i->event_list, i);
			emit_opcodes(&i->request_list, i);
			emit_stubs(&i->request_list, i);
		}
	}

	printf("#ifdef  __cplusplus\n"
	       "}\n"
	       "#endif\n"
	       "\n"
	       "#endif\n");
}

static void
emit_types_forward_declarations(struct protocol *protocol,
				struct wl_list *message_list)
{
	struct message *m;
	struct arg *a;
	int length;

	wl_list_for_each(m, message_list, link) {
		length = 0;
		m->all_null = 1;
		wl_list_for_each(a, &m->arg_list, link) {
			length++;
			switch (a->type) {
			case NEW_ID:
			case OBJECT:
				m->all_null = 0;
				printf("extern const struct wl_interface %s_interface;\n",
				       a->interface_name);
				break;
			default:
				break;
			}
		}

		if (m->all_null && length > protocol->null_run_length)
			protocol->null_run_length = length;
	}
}

static void
emit_null_run(struct protocol *protocol)
{
	int i;

	for (i = 0; i < protocol->null_run_length; i++)
		printf("\tNULL,\n");
}

static void
emit_types(struct protocol *protocol, struct wl_list *message_list)
{
	struct message *m;
	struct arg *a;

	wl_list_for_each(m, message_list, link) {
		if (m->all_null) {
			m->type_index = 0;
			continue;
		}

		m->type_index =
			protocol->null_run_length + protocol->type_index;
		protocol->type_index += m->arg_count;

		wl_list_for_each(a, &m->arg_list, link) {
			switch (a->type) {
			case NEW_ID:
			case OBJECT:
				if (strcmp(a->interface_name,
					   "wl_object") != 0)
					printf("\t&%s_interface,\n",
					       a->interface_name);
				else
					printf("\tNULL,\n");
				break;
			default:
				printf("\tNULL,\n");
				break;
			}
		}
	}
}

static void
emit_messages(struct wl_list *message_list,
	      struct interface *interface, const char *suffix)
{
	struct message *m;
	struct arg *a;

	if (wl_list_empty(message_list))
		return;

	printf("static const struct wl_message "
	       "%s_%s[] = {\n",
	       interface->name, suffix);

	wl_list_for_each(m, message_list, link) {
		printf("\t{ \"%s\", \"", m->name);
		wl_list_for_each(a, &m->arg_list, link) {
			switch (a->type) {
			default:
			case INT:
				printf("i");
				break;
			case NEW_ID:
				printf("n");
				break;
			case UNSIGNED:
				printf("u");
				break;
			case STRING:
				printf("s");
				break;
			case OBJECT:
				printf("o");
				break;
			case ARRAY:
				printf("a");
				break;
			case FD:
				printf("h");
				break;
			}
		}
		printf("\", types + %d },\n", m->type_index);
	}

	printf("};\n\n");
}

static void
emit_code(struct protocol *protocol)
{
	struct interface *i;

	if (protocol->copyright)
		format_copyright(protocol->copyright);

	printf("#include <stdlib.h>\n"
	       "#include <stdint.h>\n"
	       "#include \"wayland-util.h\"\n\n");

	wl_list_for_each(i, &protocol->interface_list, link) {
		emit_types_forward_declarations(protocol, &i->request_list);
		emit_types_forward_declarations(protocol, &i->event_list);
	}
	printf("\n");

	printf("static const struct wl_interface *types[] = {\n");
	emit_null_run(protocol);
	wl_list_for_each(i, &protocol->interface_list, link) {
		emit_types(protocol, &i->request_list);
		emit_types(protocol, &i->event_list);
	}
	printf("};\n\n");

	wl_list_for_each(i, &protocol->interface_list, link) {

		emit_messages(&i->request_list, i, "requests");
		emit_messages(&i->event_list, i, "events");

		printf("WL_EXPORT const struct wl_interface "
		       "%s_interface = {\n"
		       "\t\"%s\", %d,\n",
		       i->name, i->name, i->version);

		if (!wl_list_empty(&i->request_list))
			printf("\tARRAY_LENGTH(%s_requests), %s_requests,\n",
			       i->name, i->name);
		else
			printf("\t0, NULL,\n");

		if (!wl_list_empty(&i->event_list))
			printf("\tARRAY_LENGTH(%s_events), %s_events,\n",
			       i->name, i->name);
		else
			printf("\t0, NULL,\n");

		printf("};\n\n");
	}
}

int main(int argc, char *argv[])
{
	struct parse_context ctx;
	struct protocol protocol;
	int len;
	void *buf;

	if (argc != 2)
		usage(EXIT_FAILURE);

	wl_list_init(&protocol.interface_list);
	protocol.type_index = 0;
	protocol.null_run_length = 0;
	protocol.copyright = NULL;
	ctx.protocol = &protocol;

	ctx.filename = "<stdin>";
	ctx.parser = XML_ParserCreate(NULL);
	XML_SetUserData(ctx.parser, &ctx);
	if (ctx.parser == NULL) {
		fprintf(stderr, "failed to create parser\n");
		exit(EXIT_FAILURE);
	}

	XML_SetElementHandler(ctx.parser, start_element, end_element);
	XML_SetCharacterDataHandler(ctx.parser, character_data);

	do {
		buf = XML_GetBuffer(ctx.parser, XML_BUFFER_SIZE);
		len = fread(buf, 1, XML_BUFFER_SIZE, stdin);
		if (len < 0) {
			fprintf(stderr, "fread: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		XML_ParseBuffer(ctx.parser, len, len == 0);

	} while (len > 0);

	XML_ParserFree(ctx.parser);

	if (strcmp(argv[1], "client-header") == 0) {
		emit_header(&protocol, 0);
	} else if (strcmp(argv[1], "server-header") == 0) {
		emit_header(&protocol, 1);
	} else if (strcmp(argv[1], "code") == 0) {
		emit_code(&protocol);
	}

	return 0;
}
