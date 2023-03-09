#include "errors.h"

#include <stdio.h>

void print_error(enum error_type err) {
  switch (err) {
    case ERR_MEM_COPY:
      perror("An error occured while copying the buffer.");
      break;
    case ERR_SOCKET_WRITE:
      perror("An error occured while writing to socket.");
      break;
    case ERR_THREAD_CREATE:
      perror("An error occured while creating the thread.");
      break;
    case ERR_SOCKET_OPEN:
      perror("An error occured while opening the socket.");
      break;
    case ERR_SOCKET_READ:
      perror("An error occured while reading from socket.");
      break;
    case ERR_SOCKET_BIND:
      perror("An error occured while binding the socket.");
      break;
    case ERR_SOCKET_LISTEN:
      perror("An error occured while listening the socket.");
      break;
    case ERR_SOCKET_ACCEPT:
      perror("An error occured while accepting the socket.");
      break;
    case ERR_SOCKET_CONNECT:
      perror("An error occured while establishing socket connection.");
      break;
    case ERR_INVALID_ARGS:
      perror("Server must accept the valie of the port as argument.");
      break;
    case ERR_DESERIALIZATION:
      perror("An error occured while deserialization the message.");
      break;
    default:
      perror("Undefined err");
      break;
  }
}