# Makefile for broadcom dectmngr test application




%.o: %.c
	$(CC) -c $(CFLAGS) $(INCLUDE_PATHS) -o $@ $<

OBJS = dectmngr.o
# CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/bcmdrivers/broadcom/include/bcm963xx/
# CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/bcmdrivers/opensource/include/bcm963xx/
# CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/xChange/dslx_common/voice_res_gw/endpt/inc/
# CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/xChange/dslx_common/voice_res_gw/inc
# CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/xChange/dslx_common/voice_res_gw/codec
# CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/xChange/dslx_common/xchg_common/bos/publicInc/
# CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/xChange/dslx_common/voice_res_gw/casCtl/inc/
# CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/xChange/dslx_common/xchg_drivers/inc
CFLAGS += -DBOS_OS_LINUXUSER -DBOS_CFG_TIME -DNTR_SUPPORT -DBOS_CFG_SLEEP -DBOS_CFG_TASK -DRS_ENDIAN_TYPE=RS_LITTLE_ENDIAN



#
# Set include directories
#
INCLUDE_PATHS = \
		-I$(STAGING_DIR)/usr/include/natalie-dect/ \
		-I$(STAGING_DIR)/usr/include/natalie-dect/Phoenix  \
		-I$(STAGING_DIR)/usr/include/bcm963xx/bcmdrivers/broadcom/include/bcm963xx/ \
		-I$(STAGING_DIR)/usr/include/bcm963xx/bcmdrivers/opensource/include/bcm963xx/ \
		-I$(STAGING_DIR)/usr/include/bcm963xx/xChange/dslx_common/voice_res_gw/endpt/inc/ \
		-I$(STAGING_DIR)/usr/include/bcm963xx/xChange/dslx_common/voice_res_gw/inc \
		-I$(STAGING_DIR)/usr/include/bcm963xx/xChange/dslx_common/voice_res_gw/codec \
		-I$(STAGING_DIR)/usr/include/bcm963xx/xChange/dslx_common/xchg_common/bos/publicInc/ \
		-I$(STAGING_DIR)/usr/include/bcm963xx/xChange/dslx_common/voice_res_gw/casCtl/inc/ \
		-I$(STAGING_DIR)/usr/include/bcm963xx/xChange/dslx_common/xchg_drivers/inc \




all: main

dynamic: all

main: $(OBJS) dectproxy atohx dect dectmngr

dectmngr: dectmngr.o
	$(CC) $(LDFLAGS) $(INCLUDE_PATHS) -o $@ $< -levent

dect: dect.o
	$(CC) $(LDFLAGS) $(INCLUDE_PATHS) -o $@ $< -ljson-c

dectproxy: dectproxy.o
	$(CC) $(LDFLAGS) $(INCLUDE_PATHS) -o $@ $<

atohx: atohx.o
	$(CC) $(LDFLAGS) $(INCLUDE_PATHS) -o $@ $<


clean:
	rm -f dectmngr *.o
