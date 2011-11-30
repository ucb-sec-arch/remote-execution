#ifndef MESSAGES_H
#define MESSAGES_H

#define MAX_PAYLOAD 80

enum CLIENT_MSG { CLIENT_SEND_PAL, CLIENT_EXEC_PAL };
enum SERVER_MSG { SERVER_READY, SERVER_PAL_RESULT, SERVER_FINISH_PAL };

typedef struct packet {
    int hdr_type;
    int payload_len;
    char payload[MAX_PAYLOAD];
} __attribute__((packed)) packet_t;

#endif /* MESSAGES_H */
