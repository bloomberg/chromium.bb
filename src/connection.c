/*
 * Copyright © 2008 Kristian Høgsberg
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

union wl_value {
	uint32_t uint32;
	char *string;
	struct wl_object *object;
	uint32_t new_id;
	struct wl_array *array;
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
			for (i = 0; i < size; ++i)
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

#define ALIGN(p, s) (void *) ( ((intptr_t) (p) + ((s) - 1)) & ~((s) - 1) )

static int
wl_message_size_extra(const struct wl_message *message)
{
	char *extra;
	int i;

	for (i = 0, extra = NULL; message->signature[i]; i++) {
		switch (message->signature[i]) {
		case 's':
		case 'o':
		case 'n':
			extra = ALIGN(extra, sizeof (void *));
			extra += sizeof (void *);
			break;
		case 'a':
			extra = ALIGN(extra, sizeof (void *));
			extra += sizeof (void *) + sizeof (struct wl_array);
			break;
		case 'h':
			extra = ALIGN(extra, sizeof (int));
			extra += sizeof (int);
			break;
		default:
			break;
		}
	}

	return (intptr_t) extra;
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

struct wl_closure *
wl_closure_vmarshal(struct wl_object *sender,
		    uint32_t opcode, va_list ap,
		    const struct wl_message *message)
{
	struct wl_closure *closure;
	struct wl_object **objectp, *object;
	uint32_t length, aligned, *p, *start, size, *end;
	int dup_fd;
	struct wl_array **arrayp, *array;
	const char **sp, *s;
	const char *signature = message->signature;
	struct argument_details arg;
	char *extra;
	int i, count, fd, extra_size, *fd_ptr;

	/* FIXME: Match old fixed allocation for now */
	closure = malloc(sizeof *closure + 1024);
	if (closure == NULL)
		return NULL;

	extra_size = wl_message_size_extra(message);
	count = arg_count_for_signature(signature) + 2;
	extra = (char *) closure->buffer;
	start = &closure->buffer[DIV_ROUNDUP(extra_size, sizeof *p)];
	end = &closure->buffer[256];
	p = &start[2];

	closure->types[0] = &ffi_type_pointer;
	closure->types[1] = &ffi_type_pointer;

	for (i = 2; i < count; i++) {
		signature = get_next_argument(signature, &arg);

		switch (arg.type) {
		case 'f':
			closure->types[i] = &ffi_type_sint32;
			closure->args[i] = p;
			if (end - p < 1)
				goto err;
			*p++ = va_arg(ap, wl_fixed_t);
			break;
		case 'u':
			closure->types[i] = &ffi_type_uint32;
			closure->args[i] = p;
			if (end - p < 1)
				goto err;
			*p++ = va_arg(ap, uint32_t);
			break;
		case 'i':
			closure->types[i] = &ffi_type_sint32;
			closure->args[i] = p;
			if (end - p < 1)
				goto err;
			*p++ = va_arg(ap, int32_t);
			break;
		case 's':
			extra = ALIGN(extra, sizeof (void *));
			closure->types[i] = &ffi_type_pointer;
			closure->args[i] = extra;
			sp = (const char **) extra;
			extra += sizeof *sp;

			s = va_arg(ap, const char *);

			if (!arg.nullable && s == NULL)
				goto err_null;

			length = s ? strlen(s) + 1: 0;
			aligned = (length + 3) & ~3;
			if (p + aligned / sizeof *p + 1 > end)
				goto err;
			*p++ = length;

			if (length > 0) {
				memcpy(p, s, length);
				*sp = (const char *) p;
			} else
				*sp = NULL;

			memset((char *) p + length, 0, aligned - length);
			p += aligned / sizeof *p;
			break;
		case 'o':
			extra = ALIGN(extra, sizeof (void *));
			closure->types[i] = &ffi_type_pointer;
			closure->args[i] = extra;
			objectp = (struct wl_object **) extra;
			extra += sizeof *objectp;

			object = va_arg(ap, struct wl_object *);

			if (!arg.nullable && object == NULL)
				goto err_null;

			*objectp = object;
			if (end - p < 1)
				goto err;
			*p++ = object ? object->id : 0;
			break;

		case 'n':
			closure->types[i] = &ffi_type_uint32;
			closure->args[i] = p;
			object = va_arg(ap, struct wl_object *);
			if (end - p < 1)
				goto err;

			if (!arg.nullable && object == NULL)
				goto err_null;

			*p++ = object ? object->id : 0;
			break;

		case 'a':
			extra = ALIGN(extra, sizeof (void *));
			closure->types[i] = &ffi_type_pointer;
			closure->args[i] = extra;
			arrayp = (struct wl_array **) extra;
			extra += sizeof *arrayp;

			*arrayp = (struct wl_array *) extra;
			extra += sizeof **arrayp;

			array = va_arg(ap, struct wl_array *);

			if (!arg.nullable && array == NULL)
				goto err_null;

			if (array == NULL || array->size == 0) {
				if (end - p < 1)
					goto err;
				*p++ = 0;
				break;
			}
			if (p + DIV_ROUNDUP(array->size, sizeof *p) + 1 > end)
				goto err;
			*p++ = array->size;
			memcpy(p, array->data, array->size);

			(*arrayp)->size = array->size;
			(*arrayp)->alloc = array->alloc;
			(*arrayp)->data = p;

			p += DIV_ROUNDUP(array->size, sizeof *p);
			break;

		case 'h':
			extra = ALIGN(extra, sizeof (int));
			closure->types[i] = &ffi_type_sint;
			closure->args[i] = extra;
			fd_ptr = (int *) extra;
			extra += sizeof *fd_ptr;

			fd = va_arg(ap, int);
			dup_fd = wl_os_dupfd_cloexec(fd, 0);
			if (dup_fd < 0) {
				fprintf(stderr, "dup failed: %m");
				abort();
			}
			*fd_ptr = dup_fd;
			break;
		default:
			fprintf(stderr, "unhandled format code: '%c'\n",
				arg.type);
			assert(0);
			break;
		}
	}

	size = (p - start) * sizeof *p;
	start[0] = sender->id;
	start[1] = opcode | (size << 16);

	closure->start = start;
	closure->message = message;
	closure->count = count;

	ffi_prep_cif(&closure->cif, FFI_DEFAULT_ABI,
		     closure->count, &ffi_type_void, closure->types);

	return closure;

err:
	printf("request too big to marshal, maximum size is %zu\n",
	       sizeof closure->buffer);
	errno = ENOMEM;
	free(closure);

	return NULL;

err_null:
	free(closure);
	wl_log("error marshalling arguments for %s:%i.%s (signature %s): "
	       "null value passed for arg %i\n",
	       sender->interface->name, sender->id, message->name,
	       message->signature, i);
	errno = EINVAL;
	return NULL;
}

struct wl_closure *
wl_connection_demarshal(struct wl_connection *connection,
			uint32_t size,
			struct wl_map *objects,
			const struct wl_message *message)
{
	uint32_t *p, *next, *end, length, **id;
	int *fd;
	char *extra, **s;
	unsigned int i, count, extra_space;
	const char *signature = message->signature;
	struct argument_details arg;
	struct wl_array **array;
	struct wl_closure *closure;

	count = arg_count_for_signature(signature) + 2;
	if (count > ARRAY_LENGTH(closure->types)) {
		printf("too many args (%d)\n", count);
		errno = EINVAL;
		wl_connection_consume(connection, size);
		return NULL;
	}

	extra_space = wl_message_size_extra(message);
	closure = malloc(sizeof *closure + 8 + size + extra_space);
	if (closure == NULL)
		return NULL;

	closure->message = message;
	closure->types[0] = &ffi_type_pointer;
	closure->types[1] = &ffi_type_pointer;
	closure->start = closure->buffer;

	wl_connection_copy(connection, closure->buffer, size);
	p = &closure->buffer[2];
	end = (uint32_t *) ((char *) p + size);
	extra = (char *) end;
	for (i = 2; i < count; i++) {
		signature = get_next_argument(signature, &arg);

		if (p + 1 > end) {
			printf("message too short, "
			       "object (%d), message %s(%s)\n",
			       *p, message->name, message->signature);
			errno = EINVAL;
			goto err;
		}

		switch (arg.type) {
		case 'u':
			closure->types[i] = &ffi_type_uint32;
			closure->args[i] = p++;
			break;
		case 'i':
			closure->types[i] = &ffi_type_sint32;
			closure->args[i] = p++;
			break;
		case 'f':
			closure->types[i] = &ffi_type_sint32;
			closure->args[i] = p++;
			break;
		case 's':
			closure->types[i] = &ffi_type_pointer;
			length = *p++;

			next = p + DIV_ROUNDUP(length, sizeof *p);
			if (next > end) {
				printf("message too short, "
				       "object (%d), message %s(%s)\n",
				       *p, message->name, message->signature);
				errno = EINVAL;
				goto err;
			}

			extra = ALIGN(extra, sizeof (void *));
			s = (char **) extra;
			extra += sizeof *s;
			closure->args[i] = s;

			if (length == 0) {
				*s = NULL;
			} else {
				*s = (char *) p;
			}

			if (length > 0 && (*s)[length - 1] != '\0') {
				printf("string not nul-terminated, "
				       "message %s(%s)\n",
				       message->name, message->signature);
				errno = EINVAL;
				goto err;
			}
			p = next;
			break;
		case 'o':
			closure->types[i] = &ffi_type_pointer;
			extra = ALIGN(extra, sizeof (void *));
			id = (uint32_t **) extra;
			extra += sizeof *id;
			closure->args[i] = id;
			*id = p;

			if (**id == 0 && !arg.nullable) {
				printf("NULL object received on non-nullable "
				       "type, message %s(%s)\n", message->name,
				       message->signature);
				errno = EINVAL;
				goto err;
			}

			p++;
			break;
		case 'n':
			closure->types[i] = &ffi_type_pointer;
			extra = ALIGN(extra, sizeof (void *));
			id = (uint32_t **) extra;
			extra += sizeof *id;
			closure->args[i] = id;
			*id = p;

			if (**id == 0 && !arg.nullable) {
				printf("NULL new ID received on non-nullable "
				       "type, message %s(%s)\n", message->name,
				       message->signature);
				errno = EINVAL;
				goto err;
			}

			if (wl_map_reserve_new(objects, *p) < 0) {
				printf("not a valid new object id (%d), "
				       "message %s(%s)\n",
				       *p, message->name, message->signature);
				errno = EINVAL;
				goto err;
			}

			p++;
			break;
		case 'a':
			closure->types[i] = &ffi_type_pointer;
			length = *p++;

			next = p + DIV_ROUNDUP(length, sizeof *p);
			if (next > end) {
				printf("message too short, "
				       "object (%d), message %s(%s)\n",
				       *p, message->name, message->signature);
				errno = EINVAL;
				goto err;
			}

			extra = ALIGN(extra, sizeof (void *));
			array = (struct wl_array **) extra;
			extra += sizeof *array;
			closure->args[i] = array;

			*array = (struct wl_array *) extra;
			extra += sizeof **array;

			(*array)->size = length;
			(*array)->alloc = 0;
			(*array)->data = p;
			p = next;
			break;
		case 'h':
			closure->types[i] = &ffi_type_sint;

			extra = ALIGN(extra, sizeof (int));
			fd = (int *) extra;
			extra += sizeof *fd;
			closure->args[i] = fd;

			wl_buffer_copy(&connection->fds_in, fd, sizeof *fd);
			connection->fds_in.tail += sizeof *fd;
			break;
		default:
			printf("unknown type\n");
			assert(0);
			break;
		}
	}

	closure->count = i;

	ffi_prep_cif(&closure->cif, FFI_DEFAULT_ABI,
		     closure->count, &ffi_type_void, closure->types);

	wl_connection_consume(connection, size);

	return closure;

 err:
	closure->count = i;
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
	struct wl_object **object;
	const struct wl_message *message;
	const char *signature;
	struct argument_details arg;
	int i, count;
	uint32_t id;

	message = closure->message;
	signature = message->signature;
	count = arg_count_for_signature(signature) + 2;
	for (i = 2; i < count; i++) {
		signature = get_next_argument(signature, &arg);
		switch (arg.type) {
		case 'o':
			id = **(uint32_t **) closure->args[i];
			object = closure->args[i];
			*object = wl_map_lookup(objects, id);
			if (*object == WL_ZOMBIE_OBJECT) {
				/* references object we've already
				 * destroyed client side */
				*object = NULL;
			} else if (*object == NULL && id != 0) {
				printf("unknown object (%u), message %s(%s)\n",
				       id, message->name, message->signature);
				*object = NULL;
				errno = EINVAL;
				return -1;
			}

			if (*object != NULL && message->types[i-2] != NULL &&
			    !interface_equal((*object)->interface,
					     message->types[i-2])) {
				printf("invalid object (%u), type (%s), "
				       "message %s(%s)\n",
				       id, (*object)->interface->name,
				       message->name, message->signature);
				errno = EINVAL;
				return -1;
			}
		}
	}

	return 0;
}

void
wl_closure_invoke(struct wl_closure *closure,
		  struct wl_object *target, void (*func)(void), void *data)
{
	int result;

	closure->args[0] = &data;
	closure->args[1] = &target;

	ffi_call(&closure->cif, func, &result, closure->args);
}

static int
copy_fds_to_connection(struct wl_closure *closure,
		       struct wl_connection *connection)
{
	const struct wl_message *message = closure->message;
	uint32_t i, count;
	struct argument_details arg;
	const char *signature = message->signature;
	int *fd;

	count = arg_count_for_signature(signature) + 2;
	for (i = 2; i < count; i++) {
		signature = get_next_argument(signature, &arg);
		if (arg.type != 'h')
			continue;

		fd = closure->args[i];
		if (wl_connection_put_fd(connection, *fd)) {
			fprintf(stderr, "request could not be marshaled: "
				"can't send file descriptor");
			return -1;
		}
	}

	return 0;
}

int
wl_closure_send(struct wl_closure *closure, struct wl_connection *connection)
{
	uint32_t size;

	if (copy_fds_to_connection(closure, connection))
		return -1;

	size = closure->start[1] >> 16;

	return wl_connection_write(connection, closure->start, size);
}

int
wl_closure_queue(struct wl_closure *closure, struct wl_connection *connection)
{
	uint32_t size;

	if (copy_fds_to_connection(closure, connection))
		return -1;

	size = closure->start[1] >> 16;

	return wl_connection_queue(connection, closure->start, size);
}

void
wl_closure_print(struct wl_closure *closure, struct wl_object *target, int send)
{
	union wl_value *value;
	int32_t si;
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

	for (i = 2; i < closure->count; i++) {
		signature = get_next_argument(signature, &arg);
		if (i > 2)
			fprintf(stderr, ", ");

		value = closure->args[i];
		switch (arg.type) {
		case 'u':
			fprintf(stderr, "%u", value->uint32);
			break;
		case 'i':
			si = (int32_t) value->uint32;
			fprintf(stderr, "%d", si);
			break;
		case 'f':
			si = (int32_t) value->uint32;
			fprintf(stderr, "%f", wl_fixed_to_double(si));
			break;
		case 's':
			fprintf(stderr, "\"%s\"", value->string);
			break;
		case 'o':
			if (value->object)
				fprintf(stderr, "%s@%u",
					value->object->interface->name,
					value->object->id);
			else
				fprintf(stderr, "nil");
			break;
		case 'n':
			fprintf(stderr, "new id %s@",
				(closure->message->types[i - 2]) ?
				 closure->message->types[i - 2]->name :
				  "[unknown]");
			if (send && value->new_id != 0)
				fprintf(stderr, "%u", value->new_id);
			else if (!send && value->object != NULL)
				fprintf(stderr, "%u", value->object->id);
			else
				fprintf(stderr, "nil");
			break;
		case 'a':
			fprintf(stderr, "array");
			break;
		case 'h':
			fprintf(stderr, "fd %d", value->uint32);
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
