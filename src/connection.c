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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/uio.h>
#include <ffi.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>

#include "wayland-util.h"
#include "connection.h"

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

struct wl_buffer {
	char data[4096];
	int head, tail;
};

#define MASK(i) ((i) & 4095)

struct wl_closure {
	int count;
	const struct wl_message *message;
	ffi_type *types[20];
	ffi_cif cif;
	void *args[20];
	uint32_t buffer[64];
	uint32_t *start;
};

struct wl_connection {
	struct wl_buffer in, out;
	struct wl_buffer fds_in, fds_out;
	int fd;
	void *data;
	wl_connection_update_func_t update;
	struct wl_closure receive_closure, send_closure;
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
	int head, size;

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
	int head, tail;

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
	int head, tail;

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
	int tail, size;

	tail = MASK(b->tail);
	if (tail + count <= sizeof b->data) {
		memcpy(data, b->data + tail, count);
	} else {
		size = sizeof b->data - tail;
		memcpy(data, b->data + tail, size);
		memcpy((char *) data + size, b->data, count - size);
	}
}

struct wl_connection *
wl_connection_create(int fd,
		     wl_connection_update_func_t update,
		     void *data)
{
	struct wl_connection *connection;

	connection = malloc(sizeof *connection);
	if (connection == NULL)
		return NULL;
	memset(connection, 0, sizeof *connection);
	connection->fd = fd;
	connection->update = update;
	connection->data = data;

	connection->update(connection,
			   WL_CONNECTION_READABLE,
			   connection->data);

	return connection;
}

void
wl_connection_destroy(struct wl_connection *connection)
{
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

static void
close_fds(struct wl_buffer *buffer)
{
	int fds[32], i, count;
	size_t size;

	size = buffer->head - buffer->tail;
	if (size == 0)
		return;

	wl_buffer_copy(buffer, fds, size);
	count = size / sizeof fds[0];
	for (i = 0; i < count; i++)
		close(fds[i]);
	buffer->tail += size;
}

static void
decode_cmsg(struct wl_buffer *buffer, struct msghdr *msg)
{
	struct cmsghdr *cmsg;
	size_t size;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL;
	     cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
			size = cmsg->cmsg_len - CMSG_LEN(0);
			wl_buffer_put(buffer, CMSG_DATA(cmsg), size);
		}
	}
}

int
wl_connection_data(struct wl_connection *connection, uint32_t mask)
{
	struct iovec iov[2];
	struct msghdr msg;
	char cmsg[128];
	int len, count, clen;

	if (mask & WL_CONNECTION_WRITABLE) {
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
			len = sendmsg(connection->fd, &msg, MSG_NOSIGNAL);
		} while (len < 0 && errno == EINTR);

		if (len == -1 && errno == EPIPE) {
			return -1;
		} else if (len < 0) {
			fprintf(stderr,
				"write error for connection %p, fd %d: %m\n",
				connection, connection->fd);
			return -1;
		}

		close_fds(&connection->fds_out);

		connection->out.tail += len;
		if (connection->out.tail == connection->out.head)
			connection->update(connection,
					   WL_CONNECTION_READABLE,
					   connection->data);
	}

	if (mask & WL_CONNECTION_READABLE) {
		wl_buffer_put_iov(&connection->in, iov, &count);

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = iov;
		msg.msg_iovlen = count;
		msg.msg_control = cmsg;
		msg.msg_controllen = sizeof cmsg;
		msg.msg_flags = 0;

		do {
			len = recvmsg(connection->fd, &msg, MSG_CMSG_CLOEXEC);
		} while (len < 0 && errno == EINTR);

		if (len < 0) {
			fprintf(stderr,
				"read error from connection %p: %m (%d)\n",
				connection, errno);
			return -1;
		} else if (len == 0) {
			/* FIXME: Handle this better? */
			return -1;
		}

		decode_cmsg(&connection->fds_in, &msg);

		connection->in.head += len;
	}	

	return connection->in.head - connection->in.tail;
}

void
wl_connection_write(struct wl_connection *connection,
		    const void *data, size_t count)
{
	if (connection->out.head - connection->out.tail +
	    count > ARRAY_LENGTH(connection->out.data))
		wl_connection_data(connection, WL_CONNECTION_WRITABLE);

	wl_buffer_put(&connection->out, data, count);

	if (connection->out.head - connection->out.tail == count)
		connection->update(connection,
				   WL_CONNECTION_READABLE |
				   WL_CONNECTION_WRITABLE,
				   connection->data);
}

static int
wl_message_size_extra(const struct wl_message *message)
{
	int i, extra;

	for (i = 0, extra = 0; message->signature[i]; i++) {

		switch (message->signature[i]) {
		case 's':
		case 'o':
			extra += sizeof (void *);
			break;
		case 'a':
			extra += sizeof (void *) + sizeof (struct wl_array);
			break;
		case 'h':
			extra += sizeof (int);
			break;
		default:
			break;
		}
	}

	return extra;
}

struct wl_closure *
wl_connection_vmarshal(struct wl_connection *connection,
		       struct wl_object *sender,
		       uint32_t opcode, va_list ap,
		       const struct wl_message *message)
{
	struct wl_closure *closure = &connection->send_closure;
	struct wl_object **objectp, *object;
	uint32_t length, *p, *start, size;
	int dup_fd;
	struct wl_array **arrayp, *array;
	const char **sp, *s;
	char *extra;
	int i, count, fd, extra_size, *fd_ptr;

	extra_size = wl_message_size_extra(message);
	count = strlen(message->signature) + 2;
	extra = (char *) closure->buffer;
	start = &closure->buffer[DIV_ROUNDUP(extra_size, sizeof *p)];
	p = &start[2];
	for (i = 2; i < count; i++) {
		switch (message->signature[i - 2]) {
		case 'u':
			closure->types[i] = &ffi_type_uint32;
			closure->args[i] = p;
			*p++ = va_arg(ap, uint32_t);
			break;
		case 'i':
			closure->types[i] = &ffi_type_sint32;
			closure->args[i] = p;
			*p++ = va_arg(ap, int32_t);
			break;
		case 's':
			closure->types[i] = &ffi_type_pointer;
			closure->args[i] = extra;
			sp = (const char **) extra;
			extra += sizeof *sp;

			s = va_arg(ap, const char *);
			length = s ? strlen(s) + 1: 0;
			*p++ = length;

			if (length > 0)
				*sp = (const char *) p;
			else
				*sp = NULL;

			memcpy(p, s, length);
			p += DIV_ROUNDUP(length, sizeof *p);
			break;
		case 'o':
			closure->types[i] = &ffi_type_pointer;
			closure->args[i] = extra;
			objectp = (struct wl_object **) extra;
			extra += sizeof *objectp;

			object = va_arg(ap, struct wl_object *);
			*objectp = object;
			*p++ = object ? object->id : 0;
			break;

		case 'n':
			closure->types[i] = &ffi_type_uint32;
			closure->args[i] = p;
			object = va_arg(ap, struct wl_object *);
			*p++ = object->id;
			break;

		case 'a':
			closure->types[i] = &ffi_type_pointer;
			closure->args[i] = extra;
			arrayp = (struct wl_array **) extra;
			extra += sizeof *arrayp;

			*arrayp = (struct wl_array *) extra;
			extra += sizeof **arrayp;

			array = va_arg(ap, struct wl_array *);
			if (array == NULL || array->size == 0) {
				*p++ = 0;
				break;
			}
			*p++ = array->size;
			memcpy(p, array->data, array->size);

			(*arrayp)->size = array->size;
			(*arrayp)->alloc = array->alloc;
			(*arrayp)->data = p;

			p += DIV_ROUNDUP(array->size, sizeof *p);
			break;

		case 'h':
			closure->types[i] = &ffi_type_sint;
			closure->args[i] = extra;
			fd_ptr = (int *) extra;
			extra += sizeof *fd_ptr;

			fd = va_arg(ap, int);
			dup_fd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
			if (dup_fd < 0) {
				fprintf(stderr, "dup failed: %m");
				abort();
			}
			*fd_ptr = dup_fd;
			wl_buffer_put(&connection->fds_out,
				      &dup_fd, sizeof dup_fd);
			break;
		default:
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

	return closure;
}

struct wl_closure *
wl_connection_demarshal(struct wl_connection *connection,
			uint32_t size,
			struct wl_map *objects,
			const struct wl_message *message)
{
	uint32_t *p, *next, *end, length;
	int *fd;
	char *extra, **s;
	int i, count, extra_space;
	struct wl_object **object;
	struct wl_array **array;
	struct wl_closure *closure = &connection->receive_closure;

	count = strlen(message->signature) + 2;
	if (count > ARRAY_LENGTH(closure->types)) {
		printf("too many args (%d)\n", count);
		errno = EINVAL;
		wl_connection_consume(connection, size);
		return NULL;
	}

	extra_space = wl_message_size_extra(message);
	if (sizeof closure->buffer < size + extra_space) {
		printf("request too big, should malloc tmp buffer here\n");
		errno = ENOMEM;
		wl_connection_consume(connection, size);
		return NULL;
	}

	closure->message = message;
	closure->types[0] = &ffi_type_pointer;
	closure->types[1] = &ffi_type_pointer;

	wl_connection_copy(connection, closure->buffer, size);
	p = &closure->buffer[2];
	end = (uint32_t *) ((char *) p + size);
	extra = (char *) end;
	for (i = 2; i < count; i++) {
		if (p + 1 > end) {
			printf("message too short, "
			       "object (%d), message %s(%s)\n",
			       *p, message->name, message->signature);
			errno = EINVAL;
			goto err;
		}

		switch (message->signature[i - 2]) {
		case 'u':
			closure->types[i] = &ffi_type_uint32;
			closure->args[i] = p++;
			break;
		case 'i':
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
			object = (struct wl_object **) extra;
			extra += sizeof *object;
			closure->args[i] = object;

			*object = wl_map_lookup(objects, *p);
			if (*object == WL_ZOMBIE_OBJECT) {
				/* references object we've already
				 * destroyed client side */
				*object = NULL;
			} else if (*object == NULL && *p != 0) {
				printf("unknown object (%d), message %s(%s)\n",
				       *p, message->name, message->signature);
				*object = NULL;
				errno = EINVAL;
				goto err;
			}

			if (*object != NULL && message->types[i-2] != NULL &&
			    (*object)->interface != message->types[i-2]) {
				printf("invalid object (%d), type (%s), "
					"message %s(%s)\n",
				       *p, (*object)->interface->name,
				       message->name, message->signature);
				errno = EINVAL;
				goto err;
			}

			p++;
			break;
		case 'n':
			closure->types[i] = &ffi_type_uint32;
			closure->args[i] = p;
			object = wl_map_lookup(objects, *p);
			if (*p == 0 || object != NULL) {
				printf("not a new object (%d), "
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

void
wl_closure_invoke(struct wl_closure *closure,
		  struct wl_object *target, void (*func)(void), void *data)
{
	int result;

	closure->args[0] = &data;
	closure->args[1] = &target;

	ffi_call(&closure->cif, func, &result, closure->args);
}

void
wl_closure_send(struct wl_closure *closure, struct wl_connection *connection)
{
	uint32_t size;

	size = closure->start[1] >> 16;
	wl_connection_write(connection, closure->start, size);
}

void
wl_closure_print(struct wl_closure *closure, struct wl_object *target, int send)
{
	union wl_value *value;
	int i;
	struct timespec tp;
	unsigned int time;

	clock_gettime(CLOCK_REALTIME, &tp);
	time = (tp.tv_sec * 1000000L) + (tp.tv_nsec / 1000);

	fprintf(stderr, "[%10.3f] %s%s@%d.%s(",
		time / 1000.0,
		send ? " -> " : "",
		target->interface->name, target->id,
		closure->message->name);

	for (i = 2; i < closure->count; i++) {
		if (i > 2)
			fprintf(stderr, ", ");

		value = closure->args[i];
		switch (closure->message->signature[i - 2]) {
		case 'u':
			fprintf(stderr, "%u", value->uint32);
			break;
		case 'i':
			fprintf(stderr, "%d", value->uint32);
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
			fprintf(stderr, "new id %u", value->uint32);
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
}
