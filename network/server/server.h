#ifndef SERVER_H
#define SERVER_H

#include "crypt_curve.h"
#include "messages.h"

#ifdef LOGGING
    #include "logging.h"
#endif /* LOGGING */

#define MAX_CLIENTS 1
#define PAL_NAME "pal.bin"
#define PAL_RESULT_NAME "server_flicker_result"
#define PAL_INPUT_NAME "server_flicker_input"

#if DEBUG
    #define debug_printf(...) printf(__VA_ARGS__)
#else
    #define debug_printf(...)
#endif /* DEBUG */

void __attribute__((noreturn)) server_process(int server_fd);

int init_server(int port);

void send_pal_result(int client_fd);

void handle_client_pub_key(int client_fd, packet_t* packet, int len);
void handle_client_send_pal(int client_fd, packet_t* packet, int len);
void handle_client_send_input(int client_fd, packet_t* packet, int len);
void handle_client_exec_pal(int client_fd, packet_t* packet, int len);

void (*handle_msg[])(int fd, packet_t* packet, int len) =
{
    handle_client_pub_key,
    handle_client_send_pal,
    handle_client_send_input,
    handle_client_exec_pal
};

/* For encryption purpose */
char public_key[CRYPT_SIZE] = {0};
char shared_key[CRYPT_SIZE] = {0};
char private_key[CRYPT_SIZE] = {2};

#endif /* SERVER_H */
