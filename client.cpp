#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <string.h>

#include <pthread.h>

#include "errors.cpp"
#include "errors.h"
#include "message.h"

#define WELCOME_MESSAGE_SIZE 36
#define BUFFER_SIZE 256

typedef unsigned char byte;

bool type_mode;

serialized_message *serialize(struct client_message *src) {
  uint32_t offset = 0;
  uint32_t buffer_size =
      MSG_USERNAME_SSIZE + src->username_size + MSG_DATA_SSIZE + src->data_size;

  byte *dest = (byte *)malloc(sizeof(byte) * buffer_size);

  uint32_t username_size = htonl(src->username_size);
  if (memcpy(dest + offset, &username_size, MSG_USERNAME_SSIZE) == NULL) {
    print_error(ERR_MEM_COPY);
    return NULL;
  }

  offset += MSG_USERNAME_SSIZE;
  if (memcpy(dest + offset, src->username, src->username_size) == NULL) {
    print_error(ERR_MEM_COPY);
    return NULL;
  }

  offset += src->username_size;
  uint32_t data_size = htonl(src->data_size);
  if (memcpy(dest + offset, &data_size, MSG_DATA_SSIZE) == NULL) {
    print_error(ERR_MEM_COPY);
    return NULL;
  }

  offset += MSG_DATA_SSIZE;
  if (memcpy(dest + offset, src->data, src->data_size) == NULL) {
    print_error(ERR_MEM_COPY);
    return NULL;
  }

  serialized_message *s_msg =
      (serialized_message *)malloc(sizeof(uint32_t) + buffer_size);
  s_msg->size = buffer_size;
  s_msg->data = dest;

  return s_msg;
}

bool deserialize(struct server_message *dest, byte *buffer) {
  uint32_t offset = MSG_USERNAME_SSIZE;

  if (memcpy(&dest->username_size, buffer, MSG_USERNAME_SSIZE) == NULL) {
    print_error(ERR_MEM_COPY);
    return false;
  }

  dest->username_size = htonl(dest->username_size);

  dest->username = (byte *)malloc(dest->username_size * sizeof(byte));

  if (memcpy(dest->username, buffer + offset, dest->username_size) == NULL) {
    print_error(ERR_MEM_COPY);
    return false;
  }

  offset += dest->username_size;
  if (memcpy(&dest->data_size, buffer + offset, MSG_DATA_SSIZE) == NULL) {
    print_error(ERR_MEM_COPY);
    return false;
  }

  offset += MSG_DATA_SSIZE;

  dest->data_size = htonl(dest->data_size);

  dest->data = (byte *)malloc(dest->data_size * sizeof(byte));

  if (memcpy(dest->data, buffer + offset, dest->data_size) == NULL) {
    print_error(ERR_MEM_COPY);
    return false;
  }

  offset += dest->data_size;
  if (memcpy(&dest->date_size, buffer + offset, dest->data_size) == NULL) {
    print_error(ERR_MEM_COPY);
    return false;
  }

  dest->date_size = htonl(dest->date_size);

  offset += MSG_DATE_SSIZE;

  dest->date = (byte *)malloc(dest->date_size * sizeof(byte));

  if (memcpy(dest->date, buffer + offset, dest->date_size) == NULL) {
    print_error(ERR_MEM_COPY);
    return false;
  }

  return true;
}

void print_server_message(const struct server_message *const msg) {
  printf("{%s}[%s] %s\n", msg->date, msg->username, msg->data);
}

bool get_message(struct server_message *msg, int sockfd) {
  byte buffer[BUFFER_SIZE];

  memset(buffer, 0, BUFFER_SIZE);

  if (read(sockfd, buffer, BUFFER_SIZE) <= 0) {
    print_error(ERR_SOCKET_READ);
    exit(1);
  }

  if (!deserialize(msg, buffer)) {
    print_error(ERR_DESERIALIZATION);
    return false;
  }

  return true;
}

void *receive_message(void *arg) {
  int sockfd = *((int *)arg);
  size_t msg_counter;
  struct server_message *messages[BUFFER_SIZE];

  while (1) {
    struct server_message *msg =
        (struct server_message *)malloc(sizeof(struct server_message));

    msg_counter = 0;

    if (!get_message(msg, sockfd)) {
      continue;
    }

    messages[msg_counter++] = msg;

    while (type_mode) {
    }

    for (size_t i = 0; i < msg_counter; ++i) {
      print_server_message(messages[i]);
    }
  }

  for (size_t i = 0; i < msg_counter; ++i) {
    free(messages[i]->username);
    free(messages[i]->data);
    free(messages[i]->date);
    free(messages[i]);
  }
}

void init_socket(struct hostent *server, struct sockaddr_in *serv_addr,
                 uint16_t portno) {
  memset((char *)serv_addr, 0, sizeof(*serv_addr));
  serv_addr->sin_family = AF_INET;
  memcpy(server->h_addr, (char *)&serv_addr->sin_addr.s_addr,
         (size_t)server->h_length);
  serv_addr->sin_port = htons(portno);
}

void check_type_mode() {
  char c;

  c = getchar();

  if (c == 'm') {
    type_mode = !type_mode;
    getchar();
  }
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  int sockfd;
  uint16_t portno;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  pthread_t receive_message_thread;

  char buffer[256];

  memset(buffer, 0, 256);

  type_mode = false;

  struct client_message msg;

  if (argc < 3) {
    fprintf(stderr, "usage %s hostname port\n", argv[0]);
    exit(0);
  }

  portno = (uint16_t)atoi(argv[2]);

  /* Create a socket point */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    print_error(ERR_SOCKET_OPEN);
    exit(1);
  }

  server = gethostbyname(argv[1]);

  if (server == NULL) {
    fprintf(stderr, "ERROR, no such host\n");
    exit(0);
  }

  init_socket(server, &serv_addr, portno);

  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    print_error(ERR_SOCKET_CONNECT);
    exit(1);
  }

  strncpy(buffer, argv[3], 255);

  msg.username_size = strlen(argv[3]);
  msg.username = (byte *)malloc(msg.username_size * sizeof(byte));

  if (memcpy(msg.username, buffer, msg.username_size) == NULL) {
    print_error(ERR_MEM_COPY);
  }

  if (pthread_create(&receive_message_thread, NULL, &receive_message,
                     (void *)&sockfd) != 0) {
    print_error(ERR_THREAD_CREATE);
    exit(1);
  }

  while (1) {
    if (type_mode) {
      memset(buffer, 0, 256);

      if (fgets(buffer, 255, stdin) == NULL) {
        perror("ERROR reading from stdin");
        exit(1);
      }

      msg.data_size = strlen(buffer) * sizeof(byte);
      msg.data = (byte *)malloc(msg.data_size);

      if (memcpy(msg.data, buffer, msg.data_size) == NULL) {
        print_error(ERR_MEM_COPY);
        exit(1);
      }

      serialized_message *s_msg = serialize(&msg);

      if (write(sockfd, s_msg->data, s_msg->size) < 0) {
        print_error(ERR_SOCKET_WRITE);
        exit(1);
      }

      type_mode = false;

      free(msg.data);
      free(s_msg);
    } else {
      check_type_mode();
    }
  }

  return 0;
}
