#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <getopt.h>
#include <json-c/json.h>

#include "dect.h"


#define F_ACTIVATE_REG 1 << 0
#define F_DELETE_HSET  1 << 1
#define F_PING_HSET    1 << 2
#define F_GET_STATUS   1 << 3
#define F_JSON         1 << 4
#define F_LIST_HSETS   1 << 5
#define F_ULE          1 << 6
#define F_INIT         1 << 7
#define F_ZWITCH       1 << 8
#define F_RADIO        1 << 9
#define F_RELOAD_CONFIG 1 << 10
#define F_BUTTON       1 << 11



void exit_failure(const char *format, ...)
{
#define BUF_SIZE 500
	char err[BUF_SIZE], msg[BUF_SIZE];
	va_list ap;
	
	strncpy(err, strerror(errno), BUF_SIZE);

	va_start(ap, format);
	vsprintf(msg, format, ap);
	va_end(ap);
	
	fprintf(stderr, "%s: %s\n", msg, err);
	exit(EXIT_FAILURE);
}


static void status_packet(struct status_packet *p) {

	int i, j;

	for (i = 0; i < MAX_NR_HANDSETS; i++) {
		printf("hset: %2d", i + 1);

		if (p->handset[i].registered == TRUE) {
			printf("\tregistered\t");

			for (j = 0; j < 5; j++)
				printf("%2.2x ", p->handset[i].ipui[j]);

			if (p->handset[i].present == TRUE)
				printf("\tpresent");
			else 
				printf("\tnot present");

			if (p->handset[i].pinging == TRUE)
				printf("\tpinging");
			else 
				printf("\tnot pinging");


		} else {
			printf("\tnot registered");
		}
		printf("\n");

	}
	printf("\n");
	
	if (p->reg_mode == ENABLED)
		printf("reg_state: ENABLED\n");

	if (p->reg_mode == DISABLED)
		printf("reg_state: DISABLED\n");

	if (p->radio == ENABLED)
		printf("radio: ENABLED\n");

	if (p->radio == AUTO)
		printf("radio: AUTO\n");

	if (p->radio == DISABLED)
		printf("radio: DISABLED\n");
}


static void status_packet_json(struct status_packet *p) {

	int i, j;
	char buf[100];

	json_object *root = json_object_new_object();
	json_object *reg_active;
	json_object *radio_active;
	
	if (p->reg_mode == ENABLED)
		reg_active = json_object_new_boolean(1);
	else
		reg_active = json_object_new_boolean(0);

	
	json_object_object_add(root, "reg_active", reg_active);

	if (p->radio == ENABLED)
		radio_active = json_object_new_boolean(1);
	else
		radio_active = json_object_new_boolean(0);

	
	json_object_object_add(root, "radio_active", radio_active);



        json_object *hset_a = json_object_new_array();

	for (i = 0; i < MAX_NR_PHONES; i++) {
		
		if (p->handset[i].registered == TRUE) {
			json_object *handset = json_object_new_object();
			json_object *jint = json_object_new_int(i + 1);
			json_object_object_add(handset, "handset", jint);
			
			for (j = 0; j < 5; j++)
				snprintf(buf + 3*j, 6, "%2.2x ", p->handset[i].ipui[j]);

			json_object *rfpi = json_object_new_string(buf);
			json_object_object_add(handset, "rfpi", rfpi);

			json_object *present;
			if (p->handset[i].present == TRUE) {
				 present = json_object_new_boolean(1);
			} else
				present = json_object_new_boolean(0);

			json_object_object_add(handset, "present", present);

			json_object *pinging;
			if (p->handset[i].pinging == TRUE) {
				 pinging = json_object_new_boolean(1);
			} else
				pinging = json_object_new_boolean(0);

			json_object_object_add(handset, "pinging", pinging);


			json_object_array_add(hset_a, handset);

		}
	}

	json_object_object_add(root, "handsets", hset_a);


        json_object *ule_a = json_object_new_array();

	for (i = MAX_NR_PHONES; i < MAX_NR_HANDSETS - MAX_NR_PHONES; i++) {
		
		if (p->handset[i].registered == TRUE) {
			json_object *ule = json_object_new_object();
			json_object *jint = json_object_new_int(i + 1);
			json_object_object_add(ule, "handset", jint);
			
			for (j = 0; j < 5; j++)
				snprintf(buf + 3*j, 6, "%2.2x ", p->handset[i].ipui[j]);

			json_object *rfpi = json_object_new_string(buf);
			json_object_object_add(ule, "rfpi", rfpi);

			json_object *present;
			if (p->handset[i].present == TRUE) {
				 present = json_object_new_boolean(1);
			} else
				present = json_object_new_boolean(0);

			json_object_object_add(ule, "present", present);

			json_object *pinging;
			if (p->handset[i].pinging == TRUE) {
				 pinging = json_object_new_boolean(1);
			} else
				pinging = json_object_new_boolean(0);

			json_object_object_add(ule, "pinging", pinging);


			json_object_array_add(ule_a, ule);

		}
	}

	json_object_object_add(root, "ule", ule_a);



        printf ("%s",json_object_to_json_string(root));	
}



static send_packet(uint8_t s, uint8_t type, uint8_t arg) {

	client_packet *p;
	char nl = '\n';
	int sent;

	if ((p = (client_packet *)malloc(sizeof(client_packet))) == NULL)
		exit_failure("malloc");

	p->size = sizeof(client_packet);
	p->type = type;
	p->data = arg;
	
	if ((sent = send(s, p, p->size, 0)) == -1)
		exit_failure("send");

	free(p);
}



static void *get_reply(uint8_t s) {
	
	uint8_t *buf;
	int r;

	if ((buf = (client_packet *)malloc(sizeof(struct status_packet))) == NULL)
		exit_failure("malloc");

		       
	if (r = recv(s, buf, sizeof(struct status_packet), 0) == -1)
		exit_failure("recv");
	
	return buf;
}


static void dump_json(void) {

	/*Creating a json object*/
        json_object * jobj = json_object_new_object();

        /*Creating a json string*/
        json_object *jstring = json_object_new_string("Joys of Programming");

        /*Creating a json integer*/
        json_object *jint = json_object_new_int(10);

        /*Creating a json boolean*/
        json_object *jboolean = json_object_new_boolean(1);

        /*Creating a json double*/
        json_object *jdouble = json_object_new_double(2.14);

        /*Creating a json array*/
        json_object *jarray = json_object_new_array();

        /*Creating json strings*/
        json_object *jstring1 = json_object_new_string("c");
        json_object *jstring2 = json_object_new_string("c++");
        json_object *jstring3 = json_object_new_string("php");

        /*Adding the above created json strings to the array*/
        json_object_array_add(jarray,jstring1);
        json_object_array_add(jarray,jstring2);
        json_object_array_add(jarray,jstring3);

        /*Form the json object*/
        /*Each of these is like a key value pair*/
        json_object_object_add(jobj,"site_name", jstring);
        json_object_object_add(jobj,"technical_blog", jboolean);
        json_object_object_add(jobj,"average_posts", jdouble);
        json_object_object_add(jobj,"nr_posts", jint);
        json_object_object_add(jobj,"categories", jarray);

        /*Now printing the json object*/
        printf ("%s",json_object_to_json_string(jobj));
}


int establish_connection(void) {

	int s;
	struct sockaddr_in remote_addr;
	socklen_t remote_addr_size;

	/* Connect to dectd */
	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_addr.s_addr = INADDR_ANY;
	remote_addr.sin_port = 40713;
	
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		exit_failure("socket");

	if (connect(s, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0)
		exit_failure("error connecting to dectproxy");
	
	return s;
}



int main(int argc, char *argv[]) {

	int s, c, handset = 0, switch_on = 0;
	int flags = 0;
	int buttonArg = 0;

	s = establish_connection();

	/* Parse command line options */
	while ((c = getopt (argc, argv, "rd:p:sjluiz:x:cb:")) != -1) {
		switch (c) {
		case 'r':
			flags |= F_ACTIVATE_REG;
			break;
		case 'd':
			flags |= F_DELETE_HSET;
			handset = atoi(optarg);
			break;
		case 'p':
			flags |= F_PING_HSET;
			handset = atoi(optarg);
			break;
		case 'z':
			flags |= F_ZWITCH;
			switch_on = atoi(optarg);
			break;
		case 'x':
			flags |= F_RADIO;
			switch_on = atoi(optarg);
			break;
		case 's':
			flags |= F_GET_STATUS;
			break;

		case 'c':
			flags |= F_RELOAD_CONFIG;
			break;

		case 'j':
			flags |= F_JSON;
			break;

		case 'l':
			flags |= F_LIST_HSETS;
			break;

		case 'u':
			flags |= F_ULE;
			break;

		case 'i':
			flags |= F_INIT;
			break;
	
		case 'b':
			buttonArg = atoi(optarg);
			flags |= F_BUTTON;
			break;
		}
	}

	if (flags & F_ACTIVATE_REG) {
		printf("activate registration\n");
		send_packet(s, REGISTRATION, 0);
	}

	if (flags & F_DELETE_HSET) {
		printf("delete hset %d\n", handset);
		send_packet(s, DELETE_HSET, handset);
	}

	if (flags & F_PING_HSET) {
		printf("ping hset %d\n", handset);
		send_packet(s, PING_HSET, handset);
	}

	if (flags & F_ZWITCH) {
		printf("switch %d\n", switch_on);
		send_packet(s, ZWITCH, switch_on);
	}

	if (flags & F_RADIO) {
		printf("radio %d\n", switch_on);
		send_packet(s, RADIO, switch_on);
	}

	if (flags & F_RELOAD_CONFIG) {
	  printf("reload config\n");
	  send_packet(s, RELOAD_CONFIG, 0);
	}

	if (flags & F_GET_STATUS) {
		printf("get status\n");
		send_packet(s, GET_STATUS, 0);
		struct status_packet *p = get_reply(s);
		status_packet(p);
	}

	if (flags & F_JSON) {
		send_packet(s, GET_STATUS, 0);
		struct status_packet *p = get_reply(s);
		status_packet_json(p);
	}

	if (flags & F_LIST_HSETS) {
		printf("list handsets\n");
		send_packet(s, LIST_HANDSETS, 0);
	}

	if (flags & F_ULE) {
		printf("init\n");
		send_packet(s, ULE_START, 0);
	}


	if (flags & F_INIT) {
		printf("init\n");
		send_packet(s, INIT, 0);
	}

	if (flags & F_BUTTON) {
		printf("button %d\n", buttonArg);
		send_packet(s, BUTTON, buttonArg);
	}
	
	return 0;
}
