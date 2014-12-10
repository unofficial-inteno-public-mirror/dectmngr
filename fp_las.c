

#include <stdio.h>
#include <Api/CodecList/ApiCodecList.h>
#include <Api/FpCc/ApiFpCc.h>
#include <Api/FpMm/ApiFpMm.h>
#include <Api/FpNoEmission/ApiFpNoEmission.h>
#include <Api/GenEveNot/ApiGenEveNot.h>
#include <Api/Las/ApiLas.h>
#include <Api/Linux/ApiLinux.h>
#include <Api/FpUle/ApiFpUle.h>

#include "fp_mm.h"


#define ARRAY_SIZE(a) sizeof(a) / sizeof((a)[0])


typedef struct {
	rsuint8 type;
	const char *name;
} type_entry_t;
/* Private functions */

type_entry_t ApiGenEveNotEventType_list[] = {
	{ 
		.type = API_GEN_MESSAGE_WAITING,
		.name = "API_GEN_MESSAGE_WAITING",
	},
	{ 
		.type = API_GEN_MISSED_CALL,
		.name = "API_GEN_MISSED_CALL"
	},
	{ 
		.type = API_GEN_WEB_CONTENT,
		.name = "API_GEN_WEB_CONTENT",
	},
	{ 
		.type = API_GEN_LIST_CHANGE_IND,
		.name = "API_GEN_LIST_CHANGE_IND",
	},
	{ 
		.type = API_GEN_SW_UPGRADE_IND,
		.name = "API_GEN_SW_UPGRADE_IND",
	},
	{ 
		.type = API_GEN_PROPREITARY,
		.name = "API_GEN_PROPREITARY",
	},

};


type_entry_t ApiLasListIdType_list[] = {

	{ 
		.type = API_LAS_SUPPORTED_LISTS,
		.name = "API_LAS_SUPPORTED_LISTS",
	},
	{ 
		.type = API_LAS_MISSED_CALLS,
		.name = "API_LAS_MISSED_CALLS",
	},
	{ 
		.type = API_LAS_OUTGOING_CALLS,
		.name = "API_LAS_OUTGOING_CALLS",
	},
	{ 
		.type = API_LAS_INCOMING_ACCEPTED_CALLS,
		.name = "API_LAS_INCOMING_ACCEPTED_CALLS",
	},
	{ 
		.type = API_LAS_ALL_CALLS,
		.name = "API_LAS_ALL_CALLS",
	},
	{ 
		.type = API_LAS_CONTACTS,
		.name = "API_LAS_CONTACTS",
	},
	{ 
		.type = API_LAS_INTERNAL_NAMES,
		.name = "API_LAS_INTERNAL_NAMES",
	},
	{ 
		.type = API_LAS_DECT_SYSTEM_SETTINGS,
		.name = "API_LAS_DECT_SYSTEM_SETTINGS",
	},
	{ 
		.type = API_LAS_LINE_SETTINGS,
		.name = "API_LAS_LINE_SETTINGS",
	},
	{ 
		.type = API_LAS_ALL_INCOMING_CALLS,
		.name = "API_LAS_ALL_INCOMING_CALLS",
	},

};


type_entry_t ApiGenEveNotLcEventType_list[] = {
	{ 
		.type = API_GEN_LC_EVENT_NEW,
		.name = "API_GEN_LC_EVENT_NEW",
	},
	{ 
		.type = API_GEN_LC_EVENT_MODIFY,
		.name = "API_GEN_LC_EVENT_MODIFY",
	},
	{ 
		.type = API_GEN_LC_EVENT_DELETE,
		.name = "API_GEN_LC_EVENT_DELETE",
	},
	{ 
		.type = API_GEN_LC_EVENT_RS_UNREAD,
		.name = "API_GEN_LC_EVENT_RS_UNREAD",
	},
	{ 
		.type = API_GEN_LC_EVENT_RS_READ,
		.name = "API_GEN_LC_EVENT_RS_READ",
	},
};



static const char * type_to_name(rsint8 type, type_entry_t * list, int list_size) {
	
	int i;

	for (i = 0; i < list_size; i++) {
		if(list[i].type == type) {
			return list[i].name;
		}
        }

	return NULL;
}


static void fp_genevenot_event_ind( ApiFpGenevenotEventIndType * m) {

	int i;

	printf("Primitive: API_FP_GENEVENOT_EVENT_IND\n");

	printf("LineIdSubType: %x\n", m->LineIdSubType);
	printf("LineIdValue: %d\n", m->LineIdValue);
	printf("CountOfEvents: %d\n", m->CountOfEvents);
	printf("LengthOfData: %d\n", m->LengthOfData);

	ApiGenEveNotEventNotificationType * e = (ApiGenEveNotEventNotificationType *) m->Data;
	
	printf("EventType: %s\n", type_to_name(e->EventType, 
					       ApiGenEveNotEventType_list, 
					       ARRAY_SIZE(ApiGenEveNotEventType_list))); 

	/* List change event */
	if (e->EventType == API_GEN_LIST_CHANGE_IND) {
		printf("EventSubType: %s\n", type_to_name(e->EventSubType, 
							  ApiLasListIdType_list,
							  ARRAY_SIZE(ApiLasListIdType_list)));

		ApiGenEveNotListChangeAddDataType * l = (ApiGenEveNotListChangeAddDataType *) e->Data;
		printf("ListEventType: %s\n", type_to_name(l->ListEventType,
							  ApiGenEveNotLcEventType_list,
							  ARRAY_SIZE(ApiGenEveNotLcEventType_list)));
		
	}
	
	printf("EventMultiplicity: %d\n", e->EventMultiplicity);
	printf("LengthOfData: %d\n", e->LengthOfData);
	
}




/* Exported functions */

void api_fp_genevenot( ApifpccEmptySignalType * m) {

	printf("api_fp_genevenot\n");

	RosPrimitiveType primitive = m->Primitive;

	switch(primitive) {
		
	case API_FP_GENEVENOT_EVENT_REQ:
		break;

	case API_FP_GENEVENOT_EVENT_IND:
		fp_genevenot_event_ind( (ApiFpGenevenotEventIndType *) m);
		break;

	case API_FP_GENEVENOT_PP_EVENT_IND:
		break;

	case API_PP_GENEVENOT_EVENT_REQ:
		break;

	case API_PP_GENEVENOT_EVENT_IND:
		break;

	case API_FP_GENEVENOT_FEATURES_REQ:
		break;

	case API_FP_GENEVENOT_FEATURES_CFM:
		break;
	}
	
	
	return;
}
