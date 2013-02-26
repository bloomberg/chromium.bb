/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2013 Jason Ekstrand
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

#define _GNU_SOURCE

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/uio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <ffi.h>

#include "wayland-util.h"
#include "wayland-private.h"
#include "wayland-os.h"

#define DIV_ROUNDUP(n, a) ( ((n) + ((a) - 1)) / (a) )

struct wl_buffer {
	char data[4096];
	uint32_t head, tail;
};

#define MASK(i) ((i) & 4095)

#define MAX_FDS_OUT	28
#define CLEN		(CMSG_LEN(MAX_FDS_OUT * sizeof(int32_t)))

struct wl_connection {
	struct wl_buffer in, out;
	struct wl_buffer fds_in, fds_out;
	int fd;
	int want_flush;
};

static void
wl_buffer_put(struct wl_buffer *b, const void *data, size_t count)
{
	uint32_t head, size;

	head = MASK(b->head);
	if (head + count <= sizeof b->data) {
		memcpy(b->data + head, data, count);
	} else {
		size = sizeof b->data - head;
		memcpy(b->data + head, data, size);
		memcpy(b->data, (const char *) data + size, count - size);
	}

	b->head += count;
}

static void
wl_buffer_put_iov(struct wl_buffer *b, struct iovec *iov, int *count)
{
	uint32_t head, tail;

	head = MASK(b->head);
	tail = MASK(b->tail);
	if (head < tail) {
		iov[0].iov_base = b->data + head;
		iov[0].iov_len = tail - head;
		*count = 1;
	} else if (tail == 0) {
		iov[0].iov_base = b->data + head;
		iov[0].iov_len = sizeof b->data - head;
		*count = 1;
	} else {
		iov[0].iov_base = b->data + head;
		iov[0].iov_len = sizeof b->data - head;
		iov[1].iov_base = b->data;
		iov[1].iov_len = tail;
		*count = 2;
	}
}

static void
wl_buffer_get_iov(struct wl_buffer *b, struct iovec *iov, int *count)
{
	uint32_t head, tail;

	head = MASK(b->head);
	tail = MASK(b->tail);
	if (tail < head) {
		iov[0].iov_base = b->data + tail;
		iov[0].iov_len = head - tail;
		*count = 1;
	} else if (head == 0) {
		iov[0].iov_base = b->data + tail;
		iov[0].iov_len = sizeof b->data - tail;
		*count = 1;
	} else {
		iov[0].iov_base = b->data + tail;
		iov[0].iov_len = sizeof b->data - tail;
		iov[1].iov_base = b->data;
		iov[1].iov_len = head;
		*count = 2;
	}
}

static void
wl_buffer_copy(struct wl_buffer *b, void *data, size_t count)
{
	uint32_t tail, size;

	tail = MASK(b->tail);
	if (tail + count <= sizeof b->data) {
		memcpy(data, b->data + tail, count);
	} else {
		size = sizeof b->data - tail;
		memcpy(data, b->data + tail, size);
		memcpy((char *) data + size, b->data, count - size);
	}
}

static uint32_t
wl_buffer_size(struct wl_buffer *b)
{
	return b->head - b->tail;
}

struct wl_connection *
wl_connection_create(int fd)
{
	struct wl_connection *connection;

	connection = malloc(sizeof *connection);
	if (connection == NULL)
		return NULL;
	memset(connection, 0, sizeof *connection);
	connection->fd = fd;

	return connection;
}

static void
close_fds(struct wl_buffer *buffer, int max)
{
	int32_t fds[sizeof(buffer->data) / sizeof(int32_t)], i, count;
	size_t size;

	size = buffer->head - buffer->tail;
	if (size == 0)
		return;

	wl_buffer_copy(buffer, fds, size);
	count = size / sizeof fds[0];
	if (max > 0 && max < count)
		count = max;
	for (i = 0; i < count; i++)
		close(fds[i]);
	buffer->tail += size;
}

void
wl_connection_destroy(struct wl_connection *connection)
{
	close_fds(&connection->fds_out, -1);
	close_fds(&connection->fds_in, -1);
	close(connection->fd);
	free(connection);
}

void
wl_connection_copy(struct wl_connection *connection, void *data, size_t size)
{
	wl_buffer_copy(&connection->in, data, size);
}

void
wl_connection_consume(struct wl_connection *connection, size_t size)
{
	connection->in.tail += size;
}

static void
build_cmsg(struct wl_buffer *buffer, char *data, int *clen)
{
	struct cmsghdr *cmsg;
	size_t size;

	size = buffer->head - buffer->tail;
	if (size > MAX_FDS_OUT * sizeof(int32_t))
		size = MAX_FDS_OUT * sizeof(int32_t);

	if (size > 0) {
		cmsg = (struct cmsghdr *) data;
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(size);
		wl_buffer_copy(buffer, CMSG_DATA(cmsg), size);
		*clen = cmsg->cmsg_len;
	} else {
		*clen = 0;
	}
}

static int
decode_cmsg(struct wl_buffer *buffer, struct msghdr *msg)
{
	struct cmsghdr *cmsg;
	size_t size, max, i;
	int overflow = 0;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL;
	     cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (cmsg->cmsg_level != SOL_SOCKET ||
		    cmsg->cmsg_type != SCM_RIGHTS)
			continue;

		size = cmsg->cmsg_len - CMSG_LEN(0);
		max = sizeof(buffer->data) - wl_buffer_size(buffer);
		if (size > max || overflow) {
			overflow = 1;
			size /= sizeof(int32_t);
			for (i = 0; i < size; i++)
				close(((int*)CMSG_DATA(cmsg))[i]);
		} else {
			wl_buffer_put(buffer, CMSG_DATA(cmsg), size);
		}
	}

	if (overflow) {
		errno = EOVERFLOW;
		return -1;
	}

	return 0;
}

int
wl_connection_flush(struct wl_connection *connection)
{
	struct iovec iov[2];
	struct msghdr msg;
	char cmsg[CLEN];
	int len = 0, count, clen;
	uint32_t tail;

	if (!connection->want_flush)
		return 0;

	tail = connection->out.tail;
	while (connection->out.head - connection->out.tail > 0) {
		wl_buffer_get_iov(&connection->out, iov, &count);

		build_cmsg(&connection->fds_out, cmsg, &clen);

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = iov;
		msg.msg_iovlen = count;
		msg.msg_control = cmsg;
		msg.msg_controllen = clen;
		msg.msg_flags = 0;

		do {
			len = sendmsg(connection->fd, &msg,
				      MSG_NOSIGNAL | MSG_DONTWAIT);
		} while (len == -1 && errno == EINTR);

		if (len == -1)
			return -1;

		close_fds(&connection->fds_out, MAX_FDS_OUT);

		connection->out.tail += len;
	}

	connection->want_flush = 0;

	return connection->out.head - tail;
}

int
wl_connection_read(struct wl_connection *connection)
{
	struct iovec iov[2];
	struct msghdr msg;
	char cmsg[CLEN];
	int len, count, ret;

	if (wl_buffer_size(&connection->in) >= sizeof(connection->in.data)) {
		errno = EOVERFLOW;
		return -1;
	}

	wl_buffer_put_iov(&connection->in, iov, &count);

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = count;
	msg.msg_control = cmsg;
	msg.msg_controllen = sizeof cmsg;
	msg.msg_flags = 0;

	do {
		len = wl_os_recvmsg_cloexec(connection->fd, &msg, 0);
	} while (len < 0 && errno == EINTR);

	if (len <= 0)
		return len;

	ret = decode_cmsg(&connection->fds_in, &msg);
	if (ret)
		return -1;

	connection->in.head += len;

	return connection->in.head - connection->in.tail;
}

int
wl_connection_write(struct wl_connection *connection,
		    const void *data, size_t count)
{
	if (connection->out.head - connection->out.tail +
	    count > ARRAY_LENGTH(connection->out.data)) {
		connection->want_flush = 1;
		if (wl_connection_flush(connection) < 0)
			return -1;
	}

	wl_buffer_put(&connection->out, data, count);
	connection->want_flush = 1;

	return 0;
}

int
wl_connection_queue(struct wl_connection *connection,
		    const void *data, size_t count)
{
	if (connection->out.head - connection->out.tail +
	    count > ARRAY_LENGTH(connection->out.data)) {
		connection->want_flush = 1;
		if (wl_connection_flush(connection) < 0)
			return -1;
	}

	wl_buffer_put(&connection->out, data, count);

	return 0;
}

static int
wl_message_count_arrays(const struct wl_message *message)
{
	int i, arrays;

	for (i = 0, arrays = 0; message->signature[i]; i++) {
		if (message->signature[i] == 'a')
			arrays++;
	}

	return arrays;
}

static int
wl_connection_put_fd(struct wl_connection *connection, int32_t fd)
{
	if (wl_buffer_size(&connection->fds_out) == MAX_FDS_OUT * sizeof fd) {
		connection->want_flush = 1;
		if (wl_connection_flush(connection) < 0)
			return -1;
	}

	wl_buffer_put(&connection->fds_out, &fd, sizeof fd);

	return 0;
}

const char *
get_next_argument(const char *signature, struct argument_details *details)
{
	if (*signature == '?') {
		details->nullable = 1;
		signature++;
	} else
		details->nullable = 0;

	details->type = *signature;
	return signature + 1;
}

int
arg_count_for_signature(const char *signature)
{
	int count = 0;
	while (*signature) {
		if (*signature != '?')
			count++;
		signature++;
	}
	return count;
}

void
wl_argument_from_va_list(const char *signature, union wl_argument *args,
			 int count, va_list ap)
{
	int i;
	const char *sig_iter;
	struct argument_details arg;

	sig_iter = signature;
	for (i = 0; i < count; i++) {
		sig_iter = get_next_argument(sig_iter, &arg);

		switch(arg.type) {
		case 'i':
			args[i].i = va_arg(ap, int32_t);
			break;
		case 'u':
			args[i].u = va_arg(ap, uint32_t);
			break;
		case 'f':
			args[i].f = va_arg(ap, wl_fixed_t);
			break;
		case 's':
			args[i].s = va_arg(ap, const char *);
			break;
		case 'o':
			args[i].o = va_arg(ap, struct wl_object *);
			break;
		case 'n':
			args[i].o = va_arg(ap, struct wl_object *);
			break;
		case 'a':
			args[i].a = va_arg(ap, struct wl_array *);
			break;
		case 'h':
			args[i].h = va_arg(ap, int32_t);
			break;
		case '\0':
			return;
		}
	}
}

struct wl_closure *
wl_closure_marshal(struct wl_object *sender, uint32_t opcode,
		   union wl_argument *args,
		   const struct wl_message *message)
{
	struct wl_closure *closure;
	struct wl_object *object;
	int i, count, fd, dup_fd;
	const char *signature;
	struct argument_details arg;

	count = arg_count_for_signature(message->signature);
	if (count > WL_CLOSURE_MAX_ARGS) {
		printf("too many args (%d)\n", count);
		errno = EINVAL;
		return NULL;
	}

	closure = malloc(sizeof *closure);
	if (closure == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	memcpy(closure->args, args, count * sizeof *args);

	signature = message->signature;
	for (i = 0; i < count; i++) {
		signature = get_next_argument(signature, &arg);

		switch (arg.type) {
		case 'f':
		case 'u':
		case 'i':
			break;
		case 's':
			if (!arg.nullable && args[i].s == NULL)
				goto err_null;
			break;
		case 'o':
			if (!arg.nullable && args[i].o == NULL)
				goto err_null;
			break;
		case 'n':
			object = args[i].o;
			if (!arg.nullable && object == NULL)
				goto err_null;

			closure->args[i].n = object ? object->id : 0;
			break;
		case 'a':
			if (!arg.nullable && args[i].a == NULL)
				goto err_null;
			break;
		case 'h':
			fd = args[i].h;
			dup_fd = wl_os_dupfd_cloexec(fd, 0);
			if (dup_fd < 0) {
				fprintf(stderr, "dup failed: %m");
				abort();
			}
			closure->args[i].h = dup_fd;
			break;
		default:
			fprintf(stderr, "unhandled format code: '%c'\n",
				arg.type);
			assert(0);
			break;
		}
	}

	closure->sender_id = sender->id;
	closure->opcode = opcode;
	closure->message = message;
	closure->count = count;

	return closure;

err_null:
	wl_closure_destroy(closure);
	wl_log("error marshalling arguments for %s (signature %s): "
	       "null value passed for arg %i\n", message->name,
	       message->signature, i);
	errno = EINVAL;
	return NULL;
}

struct wl_closure *
wl_closure_vmarshal(struct wl_object *sender, uint32_t opcode, va_list ap,
		    const struct wl_message *message)
{
	union wl_argument args[WL_CLOSURE_MAX_ARGS];

	wl_argument_from_va_list(message->signature, args,
				 WL_CLOSURE_MAX_ARGS, ap);

	return wl_closure_marshal(sender, opcode, args, message);
}

struct wl_closure *
wl_connection_demarshal(struct wl_connection *connection,
			uint32_t size,
			struct wl_map *objects,
			const struct wl_message *message)
{
	uint32_t *p, *next, *end, length, id;
	int fd;
	char *s;
	unsigned int i, count, num_arrays;
	const char *signature;
	struct argument_details arg;
	struct wl_closure *closure;
	struct wl_array *array, *array_extra;

	count = arg_count_for_signature(message->signature);
	if (count > WL_CLOSURE_MAX_ARGS) {
		printf("too many args (%d)\n", count);
		errno = EINVAL;
		wl_connection_consume(connection, size);
		return NULL;
	}

	num_arrays = wl_message_count_arrays(message);
	closure = malloc(sizeof *closure + size + num_arrays * sizeof *array);
	if (closure == NULL) {
		errno = ENOMEM;
		wl_connection_consume(connection, size);
		return NULL;
	}

	array_extra = closure->extra;
	p = (uint32_t *)(closure->extra + num_arrays);
	end = p + size / sizeof *p;

	wl_connection_copy(connection, p, size);
	closure->sender_id = *p++;
	closure->opcode = *p++ & 0x0000ffff;

	signature = message->signature;
	for (i = 0; i < count; i++) {
		signature = get_next_argument(signature, &arg);

		if (arg.type != 'h' && p + 1 > end) {
			printf("message too short, "
			       "object (%d), message %s(%s)\n",
			       *p, message->name, message->signature);
			errno = EINVAL;
			goto err;
		}

		switch (arg.type) {
		case 'u':
			closure->args[i].u = *p++;
			break;
		case 'i':
			closure->args[i].i = *p++;
			break;
		case 'f':
			closure->args[i].f = *p++;
			break;
		case 's':
			length = *p++;

			if (length == 0) {
				closure->args[i].s = NULL;
				break;
			}

			next = p + DIV_ROUNDUP(length, sizeof *p);
			if (next > end) {
				printf("message too short, "
				       "object (%d), message %s(%s)\n",
				       closure->sender_id, message->name,
				       message->signature);
				errno = EINVAL;
				goto err;
			}

			s = (char *) p;

			if (length > 0 && s[length - 1] != '\0') {
				printf("string not nul-terminated, "
				       "message %s(%s)\n",
				       message->name, message->signature);
				errno = EINVAL;
				goto err;
			}

			closure->args[i].s = s;
			p = next;
			break;
		case 'o':
			id = *p++;
			closure->args[i].n = id;

			if (id == 0 && !arg.nullable) {
				printf("NULL object received on non-nullable "
				       "type, message %s(%s)\n", message->name,
				       message->signature);
				errno = EINVAL;
				goto err;
			}
			break;
		case 'n':
			id = *p++;
			closure->args[i].n = id;

			if (id == 0 && !arg.nullable) {
				printf("NULL new ID received on non-nullable "
				       "type, message %s(%s)\n", message->name,
				       message->signature);
				errno = EINVAL;
				goto err;
			}

			if (wl_map_reserve_new(objects, id) < 0) {
				printf("not a valid new object id (%d), "
				       "message %s(%s)\n",
				       id, message->name, message->signature);
				errno = EINVAL;
				goto err;
			}

			break;
		case 'a':
			length = *p++;

			next = p + DIV_ROUNDUP(length, sizeof *p);
			if (next > end) {
				printf("message too short, "
				       "object (%d), message %s(%s)\n",
				       closure->sender_id, message->name,
				       message->signature);
				errno = EINVAL;
				goto err;
			}

			array_extra->size = length;
			array_extra->alloc = 0;
			array_extra->data = p;

			closure->args[i].a = array_extra++;
			p = next;
			break;
		case 'h':
			wl_buffer_copy(&connection->fds_in, &fd, sizeof fd);
			connection->fds_in.tail += sizeof fd;
			closure->args[i].h = fd;
			break;
		default:
			printf("unknown type\n");
			assert(0);
			break;
		}
	}

	closure->count = count;
	closure->message = message;

	wl_connection_consume(connection, size);

	return closure;

 err:
	wl_closure_destroy(closure);
	wl_connection_consume(connection, size);

	return NULL;
}

static int
interface_equal(const struct wl_interface *a, const struct wl_interface *b)
{
	/* In most cases the pointer equality test is sufficient.
	 * However, in some cases, depending on how things are split
	 * across shared objects, we can end up with multiple
	 * instances of the interface metadata constants.  So if the
	 * pointers match, the interfaces are equal, if they don't
	 * match we have to compare the interface names. */

	return a == b || strcmp(a->name, b->name) == 0;
}

int
wl_closure_lookup_objects(struct wl_closure *closure, struct wl_map *objects)
{
	struct wl_object *object;
	const struct wl_message *message;
	const char *signature;
	struct argument_details arg;
	int i, count;
	uint32_t id;

	message = closure->message;
	signature = message->signature;
	count = arg_count_for_signature(signature);
	for (i = 0; i < count; i++) {
		signature = get_next_argument(signature, &arg);
		switch (arg.type) {
		case 'o':
			id = closure->args[i].n;
			closure->args[i].o = NULL;

			object = wl_map_lookup(objects, id);
			if (object == WL_ZOMBIE_OBJECT) {
				/* references object we've already
				 * destroyed client side */
				object = NULL;
			} else if (object == NULL && id != 0) {
				printf("unknown object (%u), message %s(%s)\n",
				       id, message->name, message->signature);
				object = NULL;
				errno = EINVAL;
				return -1;
			}

			if (object != NULL && message->types[i] != NULL &&
			    !interface_equal((object)->interface,
					     message->types[i])) {
				printf("invalid object (%u), type (%s), "
				       "message %s(%s)\n",
				       id, (object)->interface->name,
				       message->name, message->signature);
				errno = EINVAL;
				return -1;
			}
			closure->args[i].o = object;
		}
	}

	return 0;
}

static void
convert_arguments_to_ffi(const char *signature, union wl_argument *args,
			 int count, ffi_type **ffi_types, void** ffi_args)
{
	int i;
	const char *sig_iter;
	struct argument_details arg;

	sig_iter = signature;
	for (i = 0; i < count; i++) {
		sig_iter = get_next_argument(sig_iter, &arg);

		switch(arg.type) {
		case 'i':
			ffi_types[i] = &ffi_type_sint32;
			ffi_args[i] = &args[i].i;
			break;
		case 'u':
			ffi_types[i] = &ffi_type_uint32;
			ffi_args[i] = &args[i].u;
			break;
		case 'f':
			ffi_types[i] = &ffi_type_sint32;
			ffi_args[i] = &args[i].f;
			break;
		case 's':
			ffi_types[i] = &ffi_type_pointer;
			ffi_args[i] = &args[i].s;
			break;
		case 'o':
			ffi_types[i] = &ffi_type_pointer;
			ffi_args[i] = &args[i].o;
			break;
		case 'n':
			ffi_types[i] = &ffi_type_uint32;
			ffi_args[i] = &args[i].n;
			break;
		case 'a':
			ffi_types[i] = &ffi_type_pointer;
			ffi_args[i] = &args[i].a;
			break;
		case 'h':
			ffi_types[i] = &ffi_type_sint32;
			ffi_args[i] = &args[i].h;
			break;
		default:
			printf("unknown type\n");
			assert(0);
			break;
		}
	}
}


void
wl_closure_invoke(struct wl_closure *closure,
		  struct wl_object *target, void (*func)(void), void *data)
{
	int count;
	ffi_cif cif;
	ffi_type *ffi_types[WL_CLOSURE_MAX_ARGS + 2];
	void * ffi_args[WL_CLOSURE_MAX_ARGS + 2];

	count = arg_count_for_signature(closure->message->signature);

	ffi_types[0] = &ffi_type_pointer;
	ffi_args[0] = &data;
	ffi_types[1] = &ffi_type_pointer;
	ffi_args[1] = &target;

	convert_arguments_to_ffi(closure->message->signature, closure->args,
				 count, ffi_types + 2, ffi_args + 2);

	ffi_prep_cif(&cif, FFI_DEFAULT_ABI,
		     count + 2, &ffi_type_void, ffi_types);

	ffi_call(&cif, func, NULL, ffi_args);
}

static int
copy_fds_to_connection(struct wl_closure *closure,
		       struct wl_connection *connection)
{
	const struct wl_message *message = closure->message;
	uint32_t i, count;
	struct argument_details arg;
	const char *signature = message->signature;
	int fd;

	count = arg_count_for_signature(signature);
	for (i = 0; i < count; i++) {
		signature = get_next_argument(signature, &arg);
		if (arg.type != 'h')
			continue;

		fd = closure->args[i].h;
		if (wl_connection_put_fd(connection, fd)) {
			fprintf(stderr, "request could not be marshaled: "
				"can't send file descriptor");
			return -1;
		}
	}

	return 0;
}

static int
serialize_closure(struct wl_closure *closure, uint32_t *buffer,
		  size_t buffer_count)
{
	const struct wl_message *message = closure->message;
	unsigned int i, count, size;
	uint32_t *p, *end;
	struct argument_details arg;
	const char *signature;

	if (buffer_count < 2)
		goto overflow;

	p = buffer + 2;
	end = buffer + buffer_count;

	signature = message->signature;
	count = arg_count_for_signature(signature);
	for (i = 0; i < count; i++) {
		signature = get_next_argument(signature, &arg);

		if (arg.type == 'h')
			continue;

		if (p + 1 > end)
			goto overflow;

		switch (arg.type) {
		case 'u':
			*p++ = closure->args[i].u;
			break;
		case 'i':
			*p++ = closure->args[i].i;
			break;
		case 'f':
			*p++ = closure->args[i].f;
			break;
		case 'o':
			*p++ = closure->args[i].o ? closure->args[i].o->id : 0;
			break;
		case 'n':
			*p++ = closure->args[i].n;
			break;
		case 's':
			if (closure->args[i].s == NULL) {
				*p++ = 0;
				break;
			}

			size = strlen(closure->args[i].s) + 1;
			*p++ = size;

			if (p + DIV_ROUNDUP(size, sizeof *p) > end)
				goto overflow;

			memcpy(p, closure->args[i].s, size);
			p += DIV_ROUNDUP(size, sizeof *p);
			break;
		case 'a':
			if (closure->args[i].a == NULL) {
				*p++ = 0;
				break;
			}

			size = closure->args[i].a->size;
			*p++ = size;

			if (p + DIV_ROUNDUP(size, sizeof *p) > end)
				goto overflow;

			memcpy(p, closure->args[i].a->data, size);
			p += DIV_ROUNDUP(size, sizeof *p);
			break;
		default:
			break;
		}
	}

	size = (p - buffer) * sizeof *p;

	buffer[0] = closure->sender_id;
	buffer[1] = size << 16 | (closure->opcode & 0x0000ffff);

	return size;

overflow:
	errno = ERANGE;
	return -1;
}

int
wl_closure_send(struct wl_closure *closure, struct wl_connection *connection)
{
	uint32_t buffer[256];
	int size;

	if (copy_fds_to_connection(closure, connection))
		return -1;

	size = serialize_closure(closure, buffer, 256);
	if (size < 0)
		return -1;

	return wl_connection_write(connection, buffer, size);
}

int
wl_closure_queue(struct wl_closure *closure, struct wl_connection *connection)
{
	uint32_t buffer[256];
	int size;

	if (copy_fds_to_connection(closure, connection))
		return -1;

	size = serialize_closure(closure, buffer, 256);
	if (size < 0)
		return -1;

	return wl_connection_queue(connection, buffer, size);
}

void
wl_closure_print(struct wl_closure *closure, struct wl_object *target, int send)
{
	int i;
	struct argument_details arg;
	const char *signature = closure->message->signature;
	struct timespec tp;
	unsigned int time;

	clock_gettime(CLOCK_REALTIME, &tp);
	time = (tp.tv_sec * 1000000L) + (tp.tv_nsec / 1000);

	fprintf(stderr, "[%10.3f] %s%s@%u.%s(",
		time / 1000.0,
		send ? " -> " : "",
		target->interface->name, target->id,
		closure->message->name);

	for (i = 0; i < closure->count; i++) {
		signature = get_next_argument(signature, &arg);
		if (i > 0)
			fprintf(stderr, ", ");

		switch (arg.type) {
		case 'u':
			fprintf(stderr, "%u", closure->args[i].u);
			break;
		case 'i':
			fprintf(stderr, "%d", closure->args[i].i);
			break;
		case 'f':
			fprintf(stderr, "%f",
				wl_fixed_to_double(closure->args[i].f));
			break;
		case 's':
			fprintf(stderr, "\"%s\"", closure->args[i].s);
			break;
		case 'o':
			if (closure->args[i].o)
				fprintf(stderr, "%s@%u",
					closure->args[i].o->interface->name,
					closure->args[i].o->id);
			else
				fprintf(stderr, "nil");
			break;
		case 'n':
			fprintf(stderr, "new id %s@",
				(closure->message->types[i]) ?
				 closure->message->types[i]->name :
				  "[unknown]");
			if (closure->args[i].n != 0)
				fprintf(stderr, "%u", closure->args[i].n);
			else
				fprintf(stderr, "nil");
			break;
		case 'a':
			fprintf(stderr, "array");
			break;
		case 'h':
			fprintf(stderr, "fd %d", closure->args[i].h);
			break;
		}
	}

	fprintf(stderr, ")\n");
}

void
wl_closure_destroy(struct wl_closure *closure)
{
	free(closure);
}
