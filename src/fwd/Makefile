
### Application-specific constants

### Environment constants 

ARCH ?=
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

MQTT_PATH ?= ../libmqtt

ifdef SX1301MOD
APP_NAME = fwd_sx1301
LGW_PATH = ../sx1301hal
LCFLAGS = $(CFLAGS) -DSX1301MOD -DBIGENDIAN -fPIC -Iinc -I. -I../sx1301hal/inc -I../libmqtt
LLIBS := -lsx1301hal -L$(LGW_PATH) -lsqlite3 -lpthread -lm -lmpsse -L$(MQTT_PATH) -lpahomqtt3c
else ifdef HP0MOD
APP_NAME = fwd_sx1302
LGW_PATH = ../sx1302hal
LCFLAGS = $(CFLAGS) -DSX1302MOD -fPIC -Iinc -I. -I../sx1302hal/inc -I../libmqtt
LLIBS := -lsx1302hal -L$(LGW_PATH) -lsqlite3 -lpthread -lm -L$(MQTT_PATH) -lpahomqtt3c
else
APP_NAME = fwd_sx1302
LGW_PATH = ../sx1302hal
LCFLAGS = $(CFLAGS) -DSX1302MOD -DBIGENDIAN -fPIC -Iinc -I. -I../sx1302hal/inc -I../libmqtt
LLIBS := -lsx1302hal -L$(LGW_PATH) -lsqlite3 -lpthread -lm -lmpsse -L$(MQTT_PATH) -lpahomqtt3c
endif

OBJDIR = obj

INCLUDES = $(wildcard ${LGW_PATH}/inc/*.h)
INCLUDES += $(wildcard inc/*.h)

### linking options


### general build targets

all: $(APP_NAME) lbt_test_utily

clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(APP_NAME)

### Sub-modules compilation

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: src/%.c $(INCLUDES) | $(OBJDIR)
	$(CC) -c $(LCFLAGS) $< -o $@

lbt_test_utily: test/lbt_test_utily.c $(OBJDIR)/uart.o | $(OBJDIR)
	$(CC) -Iinc $^ -o $@ 

### Main program compilation and assembly

$(APP_NAME): $(OBJDIR)/parson.o $(OBJDIR)/base64.o $(OBJDIR)/jitqueue.o $(OBJDIR)/logger.o  $(OBJDIR)/ghost.o $(OBJDIR)/uart.o $(OBJDIR)/endianext.o $(OBJDIR)/semtech_serv.o $(OBJDIR)/service.o $(OBJDIR)/stats.o $(OBJDIR)/gwtraf_serv.o $(OBJDIR)/pkt_serv.o $(OBJDIR)/mqtt_serv.o $(OBJDIR)/relay_serv.o $(OBJDIR)/delay_serv.o $(OBJDIR)/db.o $(OBJDIR)/utilities.o $(OBJDIR)/lgwmm.o $(OBJDIR)/aes.o $(OBJDIR)/cmac.o $(OBJDIR)/mac-header-decode.o $(OBJDIR)/loramac-crypto.o $(OBJDIR)/timersync.o $(OBJDIR)/gwcfg.o $(OBJDIR)/fwd.o | $(OBJDIR)
	$(CC) $^ -o $@ $(LLIBS)

### test programs
### EOF
