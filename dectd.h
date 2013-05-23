#ifndef DECT_H
#define DECT_H


#include <stdint.h>

#define MAX_MAIL_SIZE 4098
#define MAX_LISTENERS 10
#define PKT_DATA_SIZE 100
#define MAX_NR_HSETS 6

#define MAX(a,b) ((a) > (b) ? (a) : (b))

typedef struct dect_state {
	int reg_state;
} dect_state;

enum reg_state {
	DISABLED,
	ENABLED,
};

/* enum boolean { */
/* 	FALSE, */
/* 	TRUE, */
/* }; */


enum packet_type {
	GET_STATUS,
	REGISTRATION,
	PING_HSET,
	DELETE_HSET,
	RESPONSE,
	DECT_PACKET,
	CLIENT_PACKET,
};

enum packet_resp_type {
	OK,
	ERROR,
	DATA,
};


#define PACKET_HEADER \
	uint32_t size; \
	uint32_t type;
	

typedef struct packet {
	PACKET_HEADER
	uint8_t data[PKT_DATA_SIZE];
} packet_t;

typedef struct client_packet {
	PACKET_HEADER
	uint8_t data;
} client_packet;


struct dect_packet {
	PACKET_HEADER
	uint8_t data[MAX_MAIL_SIZE];
};


struct hset {
	uint8_t registered;
	uint8_t present;
	uint8_t pinging;
	uint8_t ipui[5];
};




struct status_packet {
	PACKET_HEADER
	uint8_t reg_mode;
	struct hset handset[MAX_NR_HSETS];
};




typedef struct packet_header {
	PACKET_HEADER
} packet_header_t;

/* struct data_packet { */
/* 	uint32_t size; */
/* 	uint8_t type; */
/* 	uint8_t *data; */
/* }; */


#endif /* DECT_H */
