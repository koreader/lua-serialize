
OUTPUT_DIR?=.

SRC_DIR=.
LUA_SRC=lua-5.1.5.tar.gz
LUA_DIR=lua-5.1.5
COMPAT_DIR=lua-compat-5.2

SRCS=$(wildcard $(SRC_DIR)/*.c)
OBJS=$(SRCS:%.c=%.o)

INC_CFLAGS=-I$(LUA_DIR)/src -I$(COMPAT_DIR)/c-api

all: $(OUTPUT_DIR) libs

clean:
	rm -f $(OUTPUT_DIR)/*.so

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

$(LUA_DIR):
	[ ! -f $(LUA_SRC) ] && wget http://www.lua.org/ftp/lua-5.1.5.tar.gz || true
	[ `$(if $(DARWIN),md5 -q $(LUA_SRC),md5sum $(LUA_SRC))|cut -d\  -f1` != 2e115fe26e435e33b0d5c022e4490567 ] \
		&& rm $(LUA_SRC) \
		&& wget http://www.lua.org/ftp/lua-5.1.5.tar.gz || true
	tar zxf $(LUA_SRC)

LIBLUACOMPAT52=$(OUTPUT_DIR)/libluacompat52.$(if $(WIN32),dll,so)
LIBSERIALIZE=$(OUTPUT_DIR)/serialize.$(if $(WIN32),dll,so)

libs: $(LIBLUACOMPAT52) $(LIBSERIALIZE)

$(LIBLUACOMPAT52): $(COMPAT_DIR)/c-api/compat-5.2.c $(LUA_DIR)
	$(CC) $(INC_CFLAGS) $(CFLAGS) -o $@ $< -L$(OUTPUT_DIR) $(LDFLAGS)

$(LIBSERIALIZE): $(SRC_DIR)/serialize.c $(LUA_DIR)
	$(CC) $(INC_CFLAGS) $(CFLAGS) -o $@ $< -lluacompat52 -L$(OUTPUT_DIR) $(LDFLAGS)

