# Makefile for broadcom dectmngr test application


%.o: %.c
	$(CC) -c $(CFLAGS) $(INCLUDE_PATHS) -o $@ $<

OBJS = dectmngr.o DectNvsDefaultImage.o
CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/bcmdrivers/broadcom/include/bcm963xx/
CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/bcmdrivers/opensource/include/bcm963xx/
CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/xChange/dslx_common/voice_res_gw/endpt/inc/
CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/xChange/dslx_common/voice_res_gw/inc
CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/xChange/dslx_common/voice_res_gw/codec
CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/xChange/dslx_common/xchg_common/bos/publicInc/
CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/xChange/dslx_common/voice_res_gw/casCtl/inc/
CFLAGS += -I$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/xChange/dslx_common/xchg_drivers/inc
CFLAGS += -DBOS_OS_LINUXUSER -DBOS_CFG_TIME -DNTR_SUPPORT -DBOS_CFG_SLEEP -DBOS_CFG_TASK

DECTD_DIR=$(BUILD_DIR)/bcmkernel-4.12/4.12L.04/userspace/private/apps/dectd
BRCM_DIR=$(BUILD_DIR)/bcmkernel-4.12/4.12L.04
#
# Set include directories
#
INCLUDE_PATHS = -I$(DECTD_DIR)                       \
           -I$(DECTD_DIR)/inc                   \
           -I$(DECTD_DIR)/dectI/inc               \
           -I$(DECTD_DIR)/dectI/inc/Phoenix               \
           -I$(DECTD_DIR)/dectI/inc/Phoenix/Api               \
           -I$(DECTD_DIR)/dectI/inc/Phoenix/Api/CodecList     \
           -I$(DECTD_DIR)/dectI/inc/Phoenix/Api/FpCc          \
           -I$(DECTD_DIR)/dectI/inc/Phoenix/Api/FpFwu         \
           -I$(DECTD_DIR)/dectI/inc/Phoenix/Api/FpMm          \
           -I$(DECTD_DIR)/dectI/inc/Phoenix/Api/FpNoEmission  \
           -I$(DECTD_DIR)/dectI/inc/Phoenix/Api/GenEveNot     \
           -I$(DECTD_DIR)/dectI/inc/Phoenix/Api/FpGeneral     \
           -I$(DECTD_DIR)/dectI/inc/Phoenix/Api/Las           \
           -I$(DECTD_DIR)/dectI/inc/Phoenix/Api/LasDb         \
           -I$(DECTD_DIR)/dectI/inc/Phoenix/Api/Project       \
           -I$(DECTD_DIR)/dectI/inc/Phoenix/Api/Types         \
             -I$(BRCM_DIR)/userspace/public/include  \
             -I$(BRCM_DIR)/userspace/public/include/$(OALDIR) \
             -I$(BRCM_DIR)/userspace/private/include  \
             -I$(BRCM_DIR)/userspace/private/include/$(OALDIR) \
             -I$(BRCM_DIR)/xChange/dslx/apps/cfginc/xchg_common                 \
             -I$(BRCM_DIR)/xChange/dslx_common/xchg_common/bos/publicInc        \
             -I$(BRCM_DIR)/xChange/dslx_common/xchg_common/assert/cfginc        \
             -I$(BRCM_DIR)/xChange/dslx_common/xchg_common/assert/inc           \
             -I$(BRCM_DIR)/xChange/dslx_common/xchg_common/str                  \
            # -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD)  \
            # -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD)


all: main

dynamic: all

main: $(OBJS) dectproxy atohx dectd dect
	$(CC) $(LDFLAGS) $(INCLUDE_PATHS) -o dectmngr $(OBJS)

dectd: dectd.o
	$(CC) $(LDFLAGS) $(INCLUDE_PATHS) -o $@ $<

dect: dect.o
	$(CC) $(LDFLAGS) $(INCLUDE_PATHS) -o $@ $<

dectproxy: dectproxy.o
	$(CC) $(LDFLAGS) $(INCLUDE_PATHS) -o $@ $<

atohx: atohx.o
	$(CC) $(LDFLAGS) $(INCLUDE_PATHS) -o $@ $<


clean:
	rm -f dectmngr ${OBJS}
