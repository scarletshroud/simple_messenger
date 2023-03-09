#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <string.h>
#include <time.h>

#include <pthread.h>

#include "errors.cpp"
#include "errors.h"
#include "message.h"

#define DEFAULT_PORT 5001
#define MESSAGE_BUFFER_SIZE 256
#define NAME_BUFFER_SIZE 32
#define MAX_CLIENTS_AMOUNT 150

typedef unsigned char byte;

static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

struct client {
  uint8_t id;
  int sockfd;
  char name[NAME_BUFFER_SIZE];
  struct sockaddr_in addr;
};

struct clients_vector {
  uint8_t count;
  struct client *clients[MAX_CLIENTS_AMOUNT];
};

struct clients_vector cli_vec;

bool serialize(const struct server_message *src, serialized_message *s_msg) {
  uint32_t offset = 0;

  uint32_t buffer_size = MSG_USERNAME_SSIZE + src->username_size +
                         MSG_DATA_SSIZE + src->data_size + MSG_DATE_SSIZE +
                         src->date_size;

  s_msg->size = buffer_size;

  s_msg->data = (byte *)malloc(sizeof(byte) * buffer_size);

  uint32_t username_size = htonl(src->username_size);

  if (memcpy(s_msg->data + offset, &username_size, MSG_USERNAME_SSIZE) ==
      NULL) {
    return false;
  }

  offset += MSG_USERNAME_SSIZE;
  if (memcpy(s_msg->data + offset, src->username, src->username_size) == NULL) {
    return false;
  }

  offset += src->username_size;

  uint32_t data_size = htonl(src->data_size);

  if (memcpy(s_msg->data + offset, &data_size, MSG_DATA_SSIZE) == NULL) {
    return false;
  }

  offset += MSG_DATA_SSIZE;
  if (memcpy(s_msg->data + offset, src->data, src->data_size) == NULL) {
    return false;
  }

  offset += src->data_size;

  uint32_t date_size = htonl(src->date_size);

  if (memcpy(s_msg->data + offset, &date_size, MSG_DATE_SSIZE) == NULL) {
    return false;
  }

  offset += MSG_DATE_SSIZE;
  if (memcpy(s_msg->data + offset, src->date, src->date_size) == NULL) {
    return false;
  }

  return true;
}

bool deserialize(struct client_message *dest, byte *buffer) {
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

  dest->data_size = htonl(dest->data_size);

  dest->data = (byte *)malloc(dest->data_size * sizeof(byte));
  offset += MSG_DATA_SSIZE;
  if (memcpy(dest->data, buffer + offset, dest->data_size) == NULL) {
    print_error(ERR_MEM_COPY);
    return false;
  }

  return true;
}

static void add_client(struct client *cli) {
  pthread_mutex_lock(&clients_mutex);

  if (cli_vec.count <= MAX_CLIENTS_AMOUNT) {
    for (size_t i = 0; i < MAX_CLIENTS_AMOUNT; ++i) {
      if (cli_vec.clients[i] == NULL) {
        cli_vec.clients[cli_vec.count] = cli;
        cli_vec.count++;
        break;
      }
    }
  }

  pthread_mutex_unlock(&clients_mutex);
}

static void remove_client(struct client *cli) {
  pthread_mutex_lock(&clients_mutex);

  for (size_t i = 0; i < MAX_CLIENTS_AMOUNT; ++i) {
    if (cli_vec.clients[i] != NULL && cli_vec.clients[i]->id == cli->id) {
      cli_vec.clients[i] = NULL;
      cli_vec.count--;
      break;
    }
  }

  pthread_mutex_unlock(&clients_mutex);
}

static void get_str_time(char *time_str, size_t date_size) {
  time_t now = time(NULL);
  struct tm *tm_struct = localtime(&now);

  int hour = tm_struct->tm_hour;
  int minute = tm_struct->tm_min;

  snprintf(time_str, date_size, "%d:%d", hour, minute);
}

static struct server_message create_server_message(
    struct client_message *c_msg) {
  struct server_message s_msg;

  s_msg.username_size = c_msg->username_size;
  s_msg.username = c_msg->username;

  s_msg.data_size = c_msg->data_size;
  s_msg.data = c_msg->data;

  s_msg.date_size = MSG_DATE_SIZE;
  s_msg.date = (byte *)malloc(s_msg.date_size);
  get_str_time((char *)s_msg.date, MSG_DATE_SIZE);

  return s_msg;
}

static bool send_message(const struct server_message *msg) {
  pthread_mutex_lock(&clients_mutex);

  serialized_message s_msg = {0, NULL};

  if (serialize(msg, &s_msg)) {
    for (size_t i = 0; i < MAX_CLIENTS_AMOUNT; ++i) {
      if (cli_vec.clients[i] != NULL) {
        ssize_t bytes_wrote = 0;
        while (bytes_wrote < s_msg.size) {
          bytes_wrote +=
              write(cli_vec.clients[i]->sockfd, s_msg.data + bytes_wrote,
                    s_msg.size - bytes_wrote);
          if (bytes_wrote < 0) {
            print_error(ERR_SOCKET_WRITE);

            free(s_msg.data);

            pthread_mutex_unlock(&clients_mutex);

            return false;
          }
        }
      }
    }
  }

  free(s_msg.data);

  pthread_mutex_unlock(&clients_mutex);

  return true;
}

static bool message_validate(byte *buffer, uint8_t buffer_offset) {
  uint32_t username_size;
  uint32_t message_size;
  uint32_t offset = MSG_USERNAME_SSIZE;

  memcpy(&username_size, buffer, MSG_USERNAME_SSIZE);
  username_size = htonl(username_size);

  if (username_size == 0 || username_size > MESSAGE_BUFFER_SIZE) {
    return false;
  }

  if (buffer_offset < MSG_USERNAME_SSIZE + username_size) return false;

  offset += username_size;
  memcpy(&message_size, buffer + offset, MSG_DATA_SSIZE);
  message_size = htonl(message_size);

  if (message_size == 0 || message_size > MESSAGE_BUFFER_SIZE) {
    return false;
  }

  if (buffer_offset <
      MSG_USERNAME_SSIZE + username_size + MSG_DATA_SSIZE + message_size) {
    return false;
  }

  return true;
}

static void *serve_client(void *arg) {
  ssize_t bytes_read = 0;
  uint8_t buffer_offset = 0;
  byte buffer[MESSAGE_BUFFER_SIZE];

  memset(buffer, 0, MESSAGE_BUFFER_SIZE);

  struct client *cli = (struct client *)arg;

  while (1) {
    bytes_read = read(cli->sockfd, buffer + buffer_offset, MESSAGE_BUFFER_SIZE);

    if (bytes_read <= 0) {
      break;
    }

    buffer_offset += bytes_read;
    if (!message_validate(buffer, buffer_offset)) {
      continue;
    }

    struct client_message c_msg;

    if (!deserialize(&c_msg, buffer)) {
      print_error(ERR_DESERIALIZATION);
      continue;
    }

    struct server_message s_msg = create_server_message(&c_msg);

    if (!send_message(&s_msg)) {
      perror("ERROR sending message");
    }

    memset(buffer, 0, MESSAGE_BUFFER_SIZE);
    buffer_offset = 0;

    free(s_msg.date);
    free(c_msg.username);
    free(c_msg.data);
  }

  remove_client(cli);
  close(cli->sockfd);
  free(cli);
  pthread_detach(pthread_self());

  return NULL;
}

static void init_socket(struct sockaddr_in *serv_addr, const uint16_t portno) {
  memset((char *)serv_addr, 0, sizeof(*serv_addr));
  serv_addr->sin_family = AF_INET;
  serv_addr->sin_addr.s_addr = INADDR_ANY;
  serv_addr->sin_port = htons(portno);
}

static void init_client(struct client *cli, const struct sockaddr_in addr,
                        const uint8_t id, const int sockfd) {
  cli->id = id;
  cli->sockfd = sockfd;
  cli->addr = addr;
}

static bool is_clients_limit_reached(const uint8_t clients_amount) {
  return clients_amount >= MAX_CLIENTS_AMOUNT;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  int sockfd, newsockfd, portno;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;

  pthread_t client_thread;

  cli_vec.count = 0;

  if (argc != 2) {
    print_error(ERR_INVALID_ARGS);
    exit(1);
  }

  portno = (uint16_t)atoi(argv[1]);

  /* First call to socket() function */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    print_error(ERR_SOCKET_WRITE);
    exit(1);
  }

  /* Initialize socket structure */
  init_socket(&serv_addr, portno);

  /* Now bind the host address using bind() call.*/
  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    print_error(ERR_SOCKET_BIND);
    exit(1);
  }

  /* Now start listening for the clients, here process will
   * go in sleep mode and will wait for the incoming connection
   */

  if (listen(sockfd, 5) < 0) {
    print_error(ERR_SOCKET_LISTEN);
    return EXIT_FAILURE;
  }

  while (1) {
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

    if (newsockfd < 0) {
      print_error(ERR_SOCKET_ACCEPT);
      return EXIT_FAILURE;
    }

    if (is_clients_limit_reached(cli_vec.count + 1)) {
      printf("Clients amount limit reached.");
      close(newsockfd);
      continue;
    }

    struct client *cli = (struct client *)malloc(sizeof(struct client));
    init_client(cli, cli_addr, cli_vec.count, newsockfd);

    add_client(cli);

    pthread_create(&client_thread, NULL, &serve_client, (void *)cli);
  }

  return 0;
}
