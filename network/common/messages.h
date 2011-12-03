#ifndef MESSAGES_H
#define MESSAGES_H

#include "crypt_curve.h"

#define MAX_PAYLOAD 80

enum CLIENT_MSG { CLIENT_SEND_PUBKEY, CLIENT_SEND_PAL, CLIENT_SEND_INPUT, CLIENT_EXEC_PAL };
enum SERVER_MSG { SERVER_READY, SERVER_SEND_PUBKEY, SERVER_PAL_RESULT, SERVER_FINISH_PAL };

typedef struct packet {
    int hdr_type;
    int payload_len;
    char payload[MAX_PAYLOAD];
} __attribute__((packed)) packet_t;

static void send_packet(int fd, packet_t* packet, char* shared_key)
{
#ifdef USE_ENCRYPTION
    int i;

    if (*shared_key) {
        for (i = 0;i < sizeof(packet_t); i++) {
            ((char*)packet)[i] ^= shared_key[i % sizeof(CRYPT_SIZE)];
        }
    }
#endif /* USE_ENCRYPTION */

    send(fd, (void*)packet, sizeof(packet_t), 0);
}

static int recv_packet(int fd, packet_t* packet, char* shared_key)
{
    int len;
    int i;

    len = recv(fd, (void*)packet, sizeof(packet_t), 0);

#ifdef USE_ENCRYPTION
    if (*shared_key) {
        for (i = 0;i < sizeof(packet_t); i++) {
            ((char*)packet)[i] ^= shared_key[i % sizeof(CRYPT_SIZE)];
        }
    }
#endif /* USE_ENCRYPTION */

    return len;
} 

#endif /* MESSAGES_H */
