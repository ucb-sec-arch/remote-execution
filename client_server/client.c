#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "messages.h"

#define MAX_LEN 20
#define PAL_RESULT_NAME "client_pal_result"

char pal_name[MAX_LEN];

int init_client(int port, char* hostname)
{
    struct hostent *hostinfo;
    struct sockaddr_in address;
    int sockfd;

    printf("* Client program starting\n");

    /* Create a socket for the client */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    /* Name the socket, as agreed with the server */
    hostinfo = gethostbyname(hostname); /* look for host's name */
    address.sin_addr = *(struct in_addr *)*hostinfo -> h_addr_list;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    /* Connect the socket to the server's socket */
    if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("connecting");
        exit(1);
    }

    printf("* Connected to server\n");

    return sockfd;
}

void execute_pal(int sockfd)
{
    packet_t* packet;

    packet = (packet_t*)calloc(1, sizeof(packet_t));
    packet->hdr_type = CLIENT_EXEC_PAL;

    send(sockfd, (void*)packet, sizeof(packet_t), 0);
    free(packet);
}

void send_pal(int sockfd)
{
    FILE* pal = fopen(pal_name, "rb");
    packet_t* packet;
    char msg[MAX_PAYLOAD];
    int num_items;

    printf("* Sending PAL to server\n");
    fflush(stdout);

    while(!feof(pal)) {
        memset(msg, '\0', MAX_PAYLOAD * sizeof(char));
        num_items = fread(msg, sizeof(char), MAX_PAYLOAD, pal);

        packet = (packet_t*)calloc(1, sizeof(packet_t));

        packet->hdr_type = CLIENT_SEND_PAL;
        packet->payload_len = num_items * sizeof(char);
        memcpy(packet->payload, msg, packet->payload_len);

        send(sockfd, (void*)packet, sizeof(packet_t), 0);
        free(packet);
    }

    fclose(pal);

    execute_pal(sockfd);
}

void process_msg(int sockfd, packet_t* packet, int len)
{
    FILE* f;

    switch(packet->hdr_type) {
        case SERVER_READY:
            send_pal(sockfd);
            break;
        case SERVER_PAL_RESULT:
            f = fopen(PAL_RESULT_NAME, "a");
            fprintf(f, "%s", packet->payload);
            fclose(f);
            break;
        default:
            printf("! %s Invalid hdr type: %d\n", __func__, packet->hdr_type);
    }
}

void __attribute__((noreturn)) client_process(int sockfd)
{
    int fd;
    fd_set clientfds;
    packet_t packet;
    int len;

    FD_ZERO(&clientfds);
    FD_SET(sockfd, &clientfds);
    FD_SET(0, &clientfds);

    /* Now wait for messages from the server */
    while (1) {
        select(FD_SETSIZE, &clientfds, NULL, NULL, NULL);

        if (FD_ISSET(sockfd, &clientfds)) {
            memset((void*)&packet, '\0', sizeof(packet_t));
            len = recv(sockfd, (void*)&packet, sizeof(packet_t), 0);

            printf("* Receive a message type: %d\n", packet.hdr_type);
            process_msg(sockfd, &packet, len);
        }

        FD_ZERO(&clientfds);
    }
}

int main(int argc, char *argv[])
{
    int port;
    char hostname[100];
    int sockfd;

    port = atoi(argv[1]);
    strcpy(hostname,argv[2]);
    strcpy(pal_name, argv[3]);

    sockfd = init_client(port, hostname);

    client_process(sockfd);
}
