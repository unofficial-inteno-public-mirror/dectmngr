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


DECTD_DIR=$(BUILD_DIR)/bcmkernel-3.4-4.14/bcm963xx/userspace/private/apps/dectd
BRCM_DIR=$(BUILD_DIR)/bcmkernel-3.4-4.14/bcm963xx
BRITNEY=/home/jonash/Desktop/britney/Britney1108/NatalieFpCvm6362_v1108/SrcHeaders

#
# Set include directories
#
INCLUDE_PATHS = \
		-I$(BRITNEY)/ \
		-I$(BRITNEY)/Phoenix/ \
		-I$(BRCM_DIR)/userspace/public/include  \
		-I$(BRCM_DIR)/userspace/public/include/$(OALDIR) \
		-I$(BRCM_DIR)/userspace/private/include  \
		-I$(BRCM_DIR)/userspace/private/include/$(OALDIR) \
		-I$(BRCM_DIR)/xChange/dslx/apps/cfginc/xchg_common                 \
		-I$(BRCM_DIR)/xChange/dslx_common/xchg_common/bos/publicInc        \
		-I$(BRCM_DIR)/xChange/dslx_common/xchg_common/assert/cfginc        \
		-I$(BRCM_DIR)/xChange/dslx_common/xchg_common/assert/inc           \
		-I$(BRCM_DIR)/xChange/dslx_common/xchg_common/str                  \




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
