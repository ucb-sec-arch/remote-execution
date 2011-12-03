#ifndef CLIENT_H
#define CLIENT_H

#include "messages.h"
#include "crypt_curve.h"

#ifdef LOGGING
    #include "logging.h"
#endif /* LOGGING */

#define MAX_LEN 20
#define PAL_RESULT_NAME "client_flicker_result"

#if DEBUG
    #define debug_printf(...) printf(__VA_ARGS__)
#else
    #define debug_printf(...)
#endif /* DEBUG */

int init_client(int port, char* hostname);

void execute_pal(int sockfd);

void send_pal_input(int sockfd);

void __attribute__((noreturn)) client_process(int sockfd);

int create_pub_key();

void handle_server_ready(int fd, packet_t* packet, int len);
void handle_server_pub_key(int fd, packet_t* packet, int len);
void handle_server_pal_result(int fd, packet_t* packet, int len);
void handle_server_finish_pal(int fd, packet_t* packet, int len);

void (*handle_msg[])(int fd, packet_t* packet, int len) =
{
    handle_server_ready,
    handle_server_pub_key,    
    handle_server_pal_result,
    handle_server_finish_pal
};

char pal_name[MAX_LEN];
char pal_input_name[MAX_LEN];

/* For encryption purpose */
char public_key[CRYPT_SIZE] = {0};
char shared_key[CRYPT_SIZE] = {0};
char private_key[CRYPT_SIZE] = {1};

#endif /* CLIENT_H */
