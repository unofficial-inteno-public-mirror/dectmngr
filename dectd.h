#ifndef DECT_H
#define DECT_H


#include <stdint.h>

#define MAX_MAIL_SIZE 4098
#define MAX_LISTENERS 10
#define PKT_SIZE 100


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
};

enum packet_resp_type {
	OK,
	ERROR,
	DATA,
};


typedef struct packet {
	uint8_t type;
	uint8_t arg;
} packet;

#endif /* DECT_H */
