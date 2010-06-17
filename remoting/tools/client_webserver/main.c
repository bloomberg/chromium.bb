// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simple webserver that returns empty content with the requested mimetype.
//
// For example:
//   http://localhost:8080/pepper-application/x-chromoting-plugin
// Will return empty content, but with the requested mimetype:
//   Content-Type: pepper-application/x-chromoting-plugin
//
// This is useful for testing the Chromoting plugin while we wait for
// updated mimetype support to be added to Chrome.

#include "build/build_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <netinet/in.h>

#if defined (OS_POSIX)
#include <unistd.h>
#endif //  OS_POSIX

#define PORT 8080

void error(const char *msg) {
  fprintf(stderr, "ERROR - %s\n", msg);
  exit(1);
}

// Read text data from a socket to a buffer.
// Data up to the next \n char is read into the buffer.
void read_text_data(int sock, char *buffer, int buffsize) {
  int num_bytes;
  *buffer = '\0';

  for (num_bytes = 1; num_bytes < buffsize-1; num_bytes++) {
    char ch;

    int num_bytes_read = read(sock, &ch, 1);
    if (num_bytes_read == 1) {
      if (ch == '\n') {
        break;
      }
      *buffer++ = ch;
    } else if (num_bytes_read == 0) {
      break;
    } else {
      error("read_text_data failed");
    }
  }
  *buffer++ = '\0';
}

// Write data from a null-terminated buffer to a socket.
void write_data(int sock, const char *buffer) {
  int num_bytes = strlen(buffer);

  while (num_bytes > 0) {
    int num_bytes_written = write(sock, buffer, num_bytes);
    if (num_bytes_written <= 0) {
      error("write_data failed");
    }
    num_bytes -= num_bytes_written;
    buffer += num_bytes_written;
  }
}

void handle_request(int connection) {
  printf("Handling request...\n");
  char buffer[512];
  buffer[0] = '\0';

  // Read the first line of the request. This will be something like:
  //   GET /index.html HTTP/1.1
  read_text_data(connection, buffer, 512);

  char *saveptr = NULL;
  char *request = strtok_r(buffer, " ", &saveptr);
  char *resource = strtok_r(NULL, " ", &saveptr);
  char *version = strtok_r(NULL, " ", &saveptr);

  char mime_type[512];
  strncpy(mime_type, &resource[1], 511);
  mime_type[511] = '\0';

  if (strcmp(request, "GET")) {
    printf("Unknown request: 'GET %s %s'\n", request, version);
  } else {
    printf("Requesting '%s'\n", mime_type);
  }

  // Keep reading until we encounter a blank line.
  // This will skip over the Host, Connection, User-Agent, ...
  char ignore[512] = "ignore";
  while (strspn(ignore, " \n\r\t") != strlen(ignore)) {
    read_text_data(connection, ignore, sizeof(ignore));
  }

  // At this point, a normal webserver would verify that the requested
  // resource exists and then return it with the appropriate mimetype.

  // However, we return empty content, but with the requested plugin mimetype.
  write_data(connection, "HTTP/1.0 200 OK\r\n");
  write_data(connection, "Content-Type: ");
  // TODO(ajwong): Currently hardcoding the mimetype rather than parsing out of
  // URL to make it easier to invoke the plugin.  Change this back to writing
  // |mime_type| out after we get things cleaned up.
  write_data(connection, "pepper-application/x-chromoting-plugin");
  write_data(connection, "\r\n\r\n");

  // This dummy data is unused, but must be present or else the reader may hang.
  write_data(connection, "Data\n");
}

int main(int argc, char *argv[]) {
  printf("Chromoting client webserver: http://localhost:%d\n", PORT);

  signal(SIGCHLD, SIG_IGN);

  // Create socket.
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    error("Unable to open socket");
  }

  // Initialize socket address struct.
  struct sockaddr_in serv_addr;
  bzero((char*)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(PORT);

  // Bind socket.
  if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    error("Unable to bind socket");
  }

  if (listen(sock, 5) < 0) {
    error("Unable to listen to socket");
  }

  while (1) {
    int connection = accept(sock, NULL, NULL);
    if (connection < 0) {
      error("Unable to accept connection");
    }

    pid_t pid = fork();
    if (pid == 0) {
      // Child process.
      if (close(sock) < 0) {
        error("Unable to close socket in child");
      }

      // Handle the request.
      handle_request(connection);

      // Success. Exit to kill child process.
      exit(0);
    } else {
      // Parent process.
      if (close(connection) < 0) {
        error("Unable to close connection in parent");
      }
    }
  }

  return 0;
}
