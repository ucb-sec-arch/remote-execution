#ifndef SERVER_H
#define SERVER_H

#include "messages.h"

#define MAX_CLIENTS 1

#if DEBUG
    #define debug_printf(...) printf(__VA_ARGS__)
#else
    #define debug_printf(...)
#endif /* DEBUG */

static const char PAL_NAME[] = "pal.bin";

void __attribute__((noreturn)) server_process(int server_fd);

int init_server(int port);

void send_pal_result(int client_fd);

void handle_client_send_pal(int client_fd, packet_t* packet, int len);
void handle_client_exec_pal(int client_fd, packet_t* packet, int len);

void (*handle_msg[])(int fd, packet_t* packet, int len) =
{
    handle_client_send_pal,
    handle_client_exec_pal
};

#endif /* SERVER_H */
