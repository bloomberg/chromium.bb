/*
 * Copyright © 2010 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <expat.h>

#include "wayland-util.h"

static const char copyright[] =
	"/*\n"
	" * Copyright © 2010 Kristian Høgsberg\n"
	" *\n"
	" * Permission to use, copy, modify, distribute, and sell this software and its\n"
	" * documentation for any purpose is hereby granted without fee, provided that\n"
	" * the above copyright notice appear in all copies and that both that copyright\n"
	" * notice and this permission notice appear in supporting documentation, and\n"
	" * that the name of the copyright holders not be used in advertising or\n"
	" * publicity pertaining to distribution of the software without specific,\n"
	" * written prior permission.  The copyright holders make no representations\n"
	" * about the suitability of this software for any purpose.  It is provided \"as\n"
	" * is\" without express or implied warranty.\n"
	" *\n"
	" * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,\n"
	" * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO\n"
	" * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR\n"
	" * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,\n"
	" * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER\n"
	" * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE\n"
	" * OF THIS SOFTWARE.\n"
	" */\n";

static int
usage(int ret)
{
	fprintf(stderr, "usage: ./scanner [header|code]\n");
	exit(ret);
}

#define XML_BUFFER_SIZE 4096

struct protocol {
	char *name;
	char *uppercase_name;
	struct wl_list interface_list;
};

struct interface {
	char *name;
	char *uppercase_name;
	int version;
	struct wl_list request_list;
	struct wl_list event_list;
	struct wl_list link;
};

struct message {
	char *name;
	char *uppercase_name;
	struct wl_list arg_list;
	struct wl_list link;
	int destructor;
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
};

struct parse_context {
	struct protocol *protocol;
	struct interface *interface;
	struct message *message;
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

static void
start_element(void *data, const char *element_name, const char **atts)
{
	struct parse_context *ctx = data;
	struct interface *interface;
	struct message *message;
	struct arg *arg;
	const char *name, *type, *interface_name;
	int i, version;

	name = NULL;
	type = NULL;
	version = 0;
	interface_name = NULL;
	for (i = 0; atts[i]; i += 2) {
		if (strcmp(atts[i], "name") == 0)
			name = atts[i + 1];
		if (strcmp(atts[i], "version") == 0)
			version = atoi(atts[i + 1]);
		if (strcmp(atts[i], "type") == 0)
			type = atts[i + 1];
		if (strcmp(atts[i], "interface") == 0)
			interface_name = atts[i + 1];
	}

	if (strcmp(element_name, "protocol") == 0) {
		if (name == NULL) {
			fprintf(stderr, "no protocol name given\n");
			exit(EXIT_FAILURE);
		}

		ctx->protocol->name = strdup(name);
		ctx->protocol->uppercase_name = uppercase_dup(name);
	} else if (strcmp(element_name, "interface") == 0) {
		if (name == NULL) {
			fprintf(stderr, "no interface name given\n");
			exit(EXIT_FAILURE);
		}

		if (version == 0) {
			fprintf(stderr, "no interface version given\n");
			exit(EXIT_FAILURE);
		}

		interface = malloc(sizeof *interface);
		interface->name = strdup(name);
		interface->uppercase_name = uppercase_dup(name);
		interface->version = version;
		wl_list_init(&interface->request_list);
		wl_list_init(&interface->event_list);
		wl_list_insert(ctx->protocol->interface_list.prev,
			       &interface->link);
		ctx->interface = interface;
	} else if (strcmp(element_name, "request") == 0 ||
		   strcmp(element_name, "event") == 0) {
		if (name == NULL) {
			fprintf(stderr, "no request name given\n");
			exit(EXIT_FAILURE);
		}

		message = malloc(sizeof *message);
		message->name = strdup(name);
		message->uppercase_name = uppercase_dup(name);
		wl_list_init(&message->arg_list);

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
			if (interface_name == NULL) {
				fprintf(stderr, "no interface name given\n");
				exit(EXIT_FAILURE);
			}
			arg->type = NEW_ID;
			arg->interface_name = strdup(interface_name);
		} else if (strcmp(type, "object") == 0) {
			if (interface_name == NULL) {
				fprintf(stderr, "no interface name given\n");
				exit(EXIT_FAILURE);
			}
			arg->type = OBJECT;
			arg->interface_name = strdup(interface_name);
		} else {
			fprintf(stderr, "unknown type: %s\n", type);
			exit(EXIT_FAILURE);
		}

		wl_list_insert(ctx->message->arg_list.prev,
			       &arg->link);
	}
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
		printf("#define WL_%s_%s\t%d\n",
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
		printf("int ");
		break;
	case NEW_ID:
	case UNSIGNED:
		printf("uint32_t ");
		break;
	case STRING:
		printf("const char *");
		break;
	case OBJECT:
		printf("struct wl_%s *", a->interface_name);
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

	/* We provide a hand written constructor for the display object */
	if (strcmp(interface->name, "display") != 0)
		printf("static inline struct wl_%s *\n"
		       "wl_%s_create(struct wl_display *display, uint32_t id)\n"
		       "{\n"
		       "\treturn (struct wl_%s *)\n"
		       "\t\twl_proxy_create_for_id(display, &wl_%s_interface, id);\n"
		       "}\n\n",
		       interface->name,
		       interface->name,
		       interface->name,
		       interface->name);

	printf("static inline void\n"
	       "wl_%s_set_user_data(struct wl_%s *%s, void *user_data)\n"
	       "{\n"
	       "\twl_proxy_set_user_data((struct wl_proxy *) %s, user_data);\n"
	       "}\n\n",
	       interface->name, interface->name, interface->name,
	       interface->name);

	printf("static inline void *\n"
	       "wl_%s_get_user_data(struct wl_%s *%s)\n"
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

	/* And we have a hand-written display destructor */
	if (!has_destructor && strcmp(interface->name, "display") != 0)
		printf("static inline void\n"
		       "wl_%s_destroy(struct wl_%s *%s)\n"
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
			printf("static inline struct wl_%s *\n",
			       ret->interface_name);
		else
			printf("static inline void\n");

		printf("wl_%s_%s(struct wl_%s *%s",
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
			       "\t\t\t     &wl_%s_interface);\n"
			       "\tif (!%s)\n"
			       "\t\treturn NULL;\n\n",
			       ret->name,
			       ret->name,
			       interface->name, ret->interface_name,
			       ret->name);

		printf("\twl_proxy_marshal((struct wl_proxy *) %s,\n"
		       "\t\t\t WL_%s_%s",
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
			printf("\n\treturn (struct wl_%s *) %s;\n",
			       ret->interface_name, ret->name);

		printf("}\n\n");
	}
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
emit_structs(struct wl_list *message_list, struct interface *interface)
{
	struct message *m;
	struct arg *a;
	int is_interface, n;

	if (wl_list_empty(message_list))
		return;

	is_interface = message_list == &interface->request_list;
	printf("struct wl_%s_%s {\n", interface->name,
	       is_interface ? "interface" : "listener");

	wl_list_for_each(m, message_list, link) {
		printf("\tvoid (*%s)(", m->name);

		n = strlen(m->name) + 17;
		if (is_interface) {
			printf("struct wl_client *client,\n"
			       "%sstruct wl_%s *%s",
			       indent(n),
			       interface->name, interface->name);
		} else {
			printf("void *data,\n"),
			printf("%sstruct wl_%s *%s",
			       indent(n), interface->name, interface->name);
		}

		wl_list_for_each(a, &m->arg_list, link) {
			printf(",\n%s", indent(n));

			emit_type(a);
			printf("%s", a->name);
		}

		printf(");\n");
	}

	printf("};\n\n");

	if (!is_interface) {
	    printf("static inline int\n"
		   "wl_%s_add_listener(struct wl_%s *%s,\n"
		   "%sconst struct wl_%s_listener *listener, void *data)\n"
		   "{\n"
		   "\treturn wl_proxy_add_listener((struct wl_proxy *) %s,\n"
		   "%s(void (**)(void)) listener, data);\n"
		   "}\n\n",
		   interface->name, interface->name, interface->name,
		   indent(17 + strlen(interface->name)),
		   interface->name,
		   interface->name,
		   indent(37));
	}
}

static const char client_prototypes[] =
	"struct wl_proxy;\n\n"

	"extern void\n"
	"wl_proxy_marshal(struct wl_proxy *p, uint32_t opcode, ...);\n"

	"extern struct wl_proxy *\n"
	"wl_proxy_create(struct wl_proxy *factory,\n"
	"\t\tconst struct wl_interface *interface);\n"

	"extern struct wl_proxy *\n"
	"wl_proxy_create_for_id(struct wl_display *display,\n"
	"\t\t       const struct wl_interface *interface, uint32_t id);\n"

	"extern void\n"
	"wl_proxy_destroy(struct wl_proxy *proxy);\n\n"

	"extern int\n"
	"wl_proxy_add_listener(struct wl_proxy *proxy,\n"
	"\t\t      void (**implementation)(void), void *data);\n\n"

	"extern void\n"
	"wl_proxy_set_user_data(struct wl_proxy *proxy, void *user_data);\n\n"

	"extern void *\n"
	"wl_proxy_get_user_data(struct wl_proxy *proxy);\n\n";


static void
emit_header(struct protocol *protocol, int server)
{
	struct interface *i;
	const char *s = server ? "SERVER" : "CLIENT";

	printf("%s\n\n"
	       "#ifndef %s_%s_PROTOCOL_H\n"
	       "#define %s_%s_PROTOCOL_H\n"
	       "\n"
	       "#ifdef  __cplusplus\n"
	       "extern \"C\" {\n"
	       "#endif\n"
	       "\n"
	       "#include <stdint.h>\n"
	       "#include <stddef.h>\n"
	       "#include \"wayland-util.h\"\n\n"
	       "struct wl_client;\n\n",
	       copyright,
	       protocol->uppercase_name, s,
	       protocol->uppercase_name, s);

	wl_list_for_each(i, &protocol->interface_list, link)
		printf("struct wl_%s;\n", i->name);
	printf("\n");

	if (!server)
		printf(client_prototypes);

	wl_list_for_each(i, &protocol->interface_list, link) {
		printf("extern const struct wl_interface "
		       "wl_%s_interface;\n",
		       i->name);
	}
	printf("\n");

	wl_list_for_each(i, &protocol->interface_list, link) {

		if (server) {
			emit_structs(&i->request_list, i);
			emit_opcodes(&i->event_list, i);
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
		printf("\" },\n");
	}

	printf("};\n\n");
}

static void
emit_code(struct protocol *protocol)
{
	struct interface *i;

	printf("%s\n\n"
	       "#include <stdlib.h>\n"
	       "#include <stdint.h>\n"
	       "#include \"wayland-util.h\"\n\n",
	       copyright);

	wl_list_for_each(i, &protocol->interface_list, link) {

		emit_messages(&i->request_list, i, "requests");
		emit_messages(&i->event_list, i, "events");

		printf("WL_EXPORT const struct wl_interface "
		       "wl_%s_interface = {\n"
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
	XML_Parser parser;
	int len;
	void *buf;

	if (argc != 2)
		usage(EXIT_FAILURE);

	wl_list_init(&protocol.interface_list);
	ctx.protocol = &protocol;

	parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &ctx);
	if (parser == NULL) {
		fprintf(stderr, "failed to create parser\n");
		exit(EXIT_FAILURE);
	}

	XML_SetElementHandler(parser, start_element, NULL);
	do {
		buf = XML_GetBuffer(parser, XML_BUFFER_SIZE);
		len = fread(buf, 1, XML_BUFFER_SIZE, stdin);
		if (len < 0) {
			fprintf(stderr, "fread: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		XML_ParseBuffer(parser, len, len == 0);

	} while (len > 0);

	XML_ParserFree(parser);

	if (strcmp(argv[1], "client-header") == 0) {
		emit_header(&protocol, 0);
	} else if (strcmp(argv[1], "server-header") == 0) {
		emit_header(&protocol, 1);
	} else if (strcmp(argv[1], "code") == 0) {
		emit_code(&protocol);
	}

	return 0;
}
