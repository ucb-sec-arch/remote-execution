#ifndef CLIENT_H
#define CLIENT_H

#define MAX_LEN 20
#define PAL_RESULT_NAME "client_flicker_result"

#if DEBUG
    #define debug_printf(...) printf(__VA_ARGS__)
#else
    #define debug_printf(...)
#endif /* DEBUG */

char pal_name[MAX_LEN];

int init_client(int port, char* hostname);

void execute_pal(int sockfd);

void __attribute__((noreturn)) client_process(int sockfd);

void handle_server_ready(int fd, packet_t* packet, int len);
void handle_server_pal_result(int fd, packet_t* packet, int len);
void handle_server_finish_pal(int fd, packet_t* packet, int len);

void (*handle_msg[])(int fd, packet_t* packet, int len) =
{
    handle_server_ready,
    handle_server_pal_result,
    handle_server_finish_pal
};

#endif /* CLIENT_H */
