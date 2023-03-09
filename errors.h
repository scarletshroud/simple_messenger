#ifndef _ERRORS_H_
#define _ERRORS_H_

// [ERR_SOCKET_OPEN] = "An error occured while opening the socket.",
//     [ERR_SOCKET_READ] = "An error occured while reading from socket.",
//     [ERR_SOCKET_WRITE] = "An error occured while writing to socket.", 
//     [ERR_SOCKET_BIND] = "An error occured while binding the socket.",
//     [ERR_SOCKET_LISTEN] = "An error occured while listening the socket.",
//     [ERR_SOCKET_ACCEPT] = "An error occured while accepting the socket.", 
//     [ERR_SOCKET_CONNECT] = "An error occured while establishing socket connection.", 
//     [ERR_MEM_COPY] = "An error occured while copying the buffer.",

enum error_type { 
    ERR_SOCKET_OPEN, 
    ERR_SOCKET_READ,
    ERR_SOCKET_WRITE,
    ERR_SOCKET_BIND,
    ERR_SOCKET_LISTEN, 
    ERR_SOCKET_ACCEPT,
    ERR_SOCKET_CONNECT,
    ERR_MEM_COPY,
    ERR_THREAD_CREATE,
    ERR_INVALID_ARGS,
    ERR_DESERIALIZATION
};

void print_error(enum error_type err); 

#endif 