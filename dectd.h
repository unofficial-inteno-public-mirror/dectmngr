#ifndef DECT_H
#define DECT_H


#include <stdint.h>

#define MAX_MAIL_SIZE 4098
#define MAX_LISTENERS 10
#define PKT_DATA_SIZE 100


#define MAX(a,b) ((a) > (b) ? (a) : (b))

typedef struct dect_state {
	int reg_state;
} dect_state;

enum reg_state {
	ENABLED,
	DISABLED,
};


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



typedef struct packet_header {
	PACKET_HEADER
} packet_header_t;

/* struct data_packet { */
/* 	uint32_t size; */
/* 	uint8_t type; */
/* 	uint8_t *data; */
/* }; */


#endif /* DECT_H */
