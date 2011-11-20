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
    int server_sockfd;
    struct sockaddr_in server_address;
    int addresslen = sizeof(struct sockaddr_in);

    printf("* Server program starting\n");

    /* Create and name a socket for the server */
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);
    bind(server_sockfd, (struct sockaddr *)&server_address, addresslen);

    /* Create a connection queue and initialize a file descriptor set */
    listen(server_sockfd, 1);

    return server_sockfd;
}

void execute_flicker()
{
    system("./go.sh > flicker_result");
}

void process_msg(packet_t* packet, int len)
{
    FILE* f;

    switch(packet->hdr_type) {
        case SENDING_PAL:
            f = fopen(PAL_NAME, "ab");
            fwrite(packet->payload, sizeof(char), packet->payload_len / sizeof(char), f);
            fclose(f);

            break;
        case EXEC_PAL:
            execute_flicker();
    }
}

void __attribute__((noreturn)) server_process(int server_sockfd)
{
    int num_clients = 0;
    char fd_array[MAX_CLIENTS];
    int client_sockfd;
    int fd;
    fd_set readfds, testfds;
    packet_t packet;
    int len;

    FD_ZERO(&readfds);
    FD_SET(server_sockfd, &readfds);

    /* Now wait for clients and requests */
    while (1) {
        testfds = readfds;
        select(FD_SETSIZE, &testfds, NULL, NULL, NULL);

        /* If there is activity, find which descriptor it's on using FD_ISSET */
        for (fd = 0; fd < FD_SETSIZE; fd++) {
            if (FD_ISSET(fd, &testfds)) {

                /* Accept a new connection request */
                if (fd == server_sockfd) {
                    client_sockfd = accept(server_sockfd, NULL, NULL);

                    if (num_clients < MAX_CLIENTS) {

                        FD_SET(client_sockfd, &readfds);
                        fd_array[num_clients]=client_sockfd;

                        printf("* Client %d joined\n",num_clients++);

                        packet_t first_packet;
                        first_packet.hdr_type = SERVER_READY;

                        send(client_sockfd, (void*)&first_packet, sizeof(packet_t), 0);
                    } else {
                       printf("! %s Too many clients\n", __func__);
                       close(client_sockfd);
                    }
                } else if (fd) { /* Process client specific activity */
                    memset((void*)&packet, '\0', sizeof(packet_t));
                    len = recv(fd, (void*)&packet, sizeof(packet_t), 0);

                    process_msg(&packet, len);
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
