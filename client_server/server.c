#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "messages.h"

#define MAX_CLIENTS 1

static const char PAL_NAME[] = "pal.bin";

int init_server(int port)
{
    int server_fd;
    struct sockaddr_in server_address;
    int addresslen = sizeof(struct sockaddr_in);

    printf("* Server program starting\n");

    /* Create and name a socket for the server */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);
    bind(server_fd, (struct sockaddr *)&server_address, addresslen);

    /* Create a connection queue and initialize a file descriptor set */
    listen(server_fd, 1);

    return server_fd;
}

void execute_flicker(int client_fd)
{
    FILE* pal_result;
    packet_t* packet;

    system("./go.sh > server_flicker_result");

    printf("* Sending PAL result to client\n");

    pal_result = fopen("server_flicker_result", "r");

    while (!feof(pal_result)) {
        packet = (packet_t*)calloc(1, sizeof(packet_t));
        packet->hdr_type = SERVER_PAL_RESULT;
        fgets(packet->payload, MAX_PAYLOAD, pal_result);
        packet->payload_len = strlen(packet->payload);

        send(client_fd, (void*)packet, sizeof(packet_t), 0);
        free(packet);
    }
}

void process_msg(int client_fd, packet_t* packet, int len)
{
    FILE* f;

    switch(packet->hdr_type) {
        case CLIENT_SEND_PAL:
            f = fopen(PAL_NAME, "ab");
            fwrite(packet->payload, sizeof(char), packet->payload_len / sizeof(char), f);
            fclose(f);

            break;
        case CLIENT_EXEC_PAL:
            execute_flicker(client_fd);
    }
}

void __attribute__((noreturn)) server_process(int server_fd)
{
    int num_clients = 0;
    char fd_array[MAX_CLIENTS];
    int client_fd;
    int fd;
    fd_set readfds, testfds;
    packet_t packet;
    int len;

    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);

    /* Now wait for clients and requests */
    while (1) {
        testfds = readfds;
        select(FD_SETSIZE, &testfds, NULL, NULL, NULL);

        /* If there is activity, find which descriptor it's on using FD_ISSET */
        for (fd = 0; fd < FD_SETSIZE; fd++) {
            if (FD_ISSET(fd, &testfds)) {

                /* Accept a new connection request */
                if (fd == server_fd) {
                    client_fd = accept(server_fd, NULL, NULL);

                    if (num_clients < MAX_CLIENTS) {

                        FD_SET(client_fd, &readfds);
                        fd_array[num_clients]=client_fd;

                        printf("* Client %d joined\n",num_clients++);

                        packet_t first_packet;
                        first_packet.hdr_type = SERVER_READY;

                        send(client_fd, (void*)&first_packet, sizeof(packet_t), 0);
                    } else {
                       printf("! %s Too many clients\n", __func__);
                       close(client_fd);
                    }
                } else if (fd) { /* Process client specific activity */
                    memset((void*)&packet, '\0', sizeof(packet_t));
                    len = recv(fd, (void*)&packet, sizeof(packet_t), 0);

                    process_msg(client_fd, &packet, len);
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    int port;
    int sockfd;

    port = atoi(argv[1]);
    sockfd = init_server(port);
    server_process(sockfd);
}
