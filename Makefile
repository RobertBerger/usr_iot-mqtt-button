include local-config.mak

DEFS+=  
INCLUDES+= -I usr/include
LIBS+= -l paho-mqtt3a


C_AND_LD_FLAGS=

CFLAGS+= -g $(C_AND_LD_FLAGS)
LDFLAGS+= $(C_AND_LD_FLAGS)


OBJS = $(PROJECT_NAME).o 

include $(MAKE_DIR)/global.mak


