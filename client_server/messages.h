#ifndef MESSAGES_H
#define MESSAGES_H

#define MAX_PAYLOAD 80

enum { SERVER_READY, SENDING_PAL, EXEC_PAL};

typedef struct packet {
    int hdr_type;
    int payload_len;
    char payload[MAX_PAYLOAD];
} __attribute__((packed)) packet_t;

#endif /* MESSAGES_H */
