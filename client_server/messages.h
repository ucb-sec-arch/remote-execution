#ifndef MESSAGES_H
#define MESSAGES_H

#define MAX_PAYLOAD 80

enum { SERVER_READY, CLIENT_SEND_PAL, CLIENT_EXEC_PAL, SERVER_PAL_RESULT};

typedef struct packet {
    int hdr_type;
    int payload_len;
    char payload[MAX_PAYLOAD];
} __attribute__((packed)) packet_t;

#endif /* MESSAGES_H */
