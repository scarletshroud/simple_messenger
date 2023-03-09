#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <stdint.h>

//Protocol defines

#define MSG_USERNAME_SSIZE 4
#define MSG_DATA_SSIZE 4
#define MSG_DATE_SSIZE 4
#define MSG_DATE_SIZE 5

typedef unsigned char byte;

#pragma pack(push, 1)

struct client_message {
  uint32_t username_size;
  byte *username;
  uint32_t data_size;
  byte *data;
};

#pragma pack(pop)

#pragma pack(push, 1)

struct server_message {
  uint32_t username_size;
  byte *username;
  uint32_t data_size;
  byte *data;
  uint32_t date_size;
  byte *date;
};

#pragma pack(pop)

struct serialized_message {
  uint32_t size; 
  byte* data; 
};

void print_client_message(const struct client_message * const msg); 

#endif