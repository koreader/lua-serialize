#include "compat-5.2.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#define TYPE_NIL 0
#define TYPE_BOOLEAN 1
// hibits 0 false 1 true
#define TYPE_NUMBER 2
// hibits 0 : 0 , 1: byte, 2:word, 4: dword, 8 : double
#define TYPE_USERDATA 3
#define TYPE_SHORT_STRING 4
// hibits 0~31 : len
#define TYPE_LONG_STRING 5
#define TYPE_TABLE 6

#define MAX_COOKIE 32
#define COMBINE_TYPE(t,v) ((t) | (v) << 3)

#define BLOCK_SIZE 128
#define MAX_DEPTH 32

struct block {
	struct block * next;
	char buffer[BLOCK_SIZE];
};

struct write_block {
	struct block * head;
	int len;
	struct block * current;
	int ptr;
};

struct read_block {
	char * buffer;
	struct block * current;
	int len;
	int ptr;
};

inline static struct block *
blk_alloc(void) {
	struct block *b = malloc(sizeof(struct block));
	b->next = NULL;
	return b;
}

inline static void
wb_push(struct write_block *b, const void *buf, int sz) {
	const char * buffer = buf;
	if (b->ptr == BLOCK_SIZE) {
_again:
		b->current = b->current->next = blk_alloc();
		b->ptr = 0;
	}
	if (b->ptr <= BLOCK_SIZE - sz) {
		memcpy(b->current->buffer + b->ptr, buffer, sz);
		b->ptr+=sz;
		b->len+=sz;
	} else {
		int copy = BLOCK_SIZE - b->ptr;
		memcpy(b->current->buffer + b->ptr, buffer, copy);
		buffer += copy;
		b->len += copy;
		sz -= copy;
		goto _again;
	}
}

static void
wb_init(struct write_block *wb , struct block *b) {
	if (b==NULL) {
		wb->head = blk_alloc();
		wb->len = 0;
		wb->current = wb->head;
		wb->ptr = 0;
		wb_push(wb, &wb->len, sizeof(wb->len));
	} else {
		wb->head = b;
		int * plen = (int *)b->buffer;
		int sz = *plen;
		wb->len = sz;
		while (b->next) {
			sz -= BLOCK_SIZE;
			b = b->next;
		}
		wb->current = b;
		wb->ptr = sz;
	}
}

static struct block *
wb_close(struct write_block *b) {
	b->current = b->head;
	b->ptr = 0;
	wb_push(b, &b->len, sizeof(b->len));
	b->current = NULL;
	return b->head;
}

static void
wb_free(struct write_block *wb) {
	struct block *blk = wb->head;
	while (blk) {
		struct block * next = blk->next;
		free(blk);
		blk = next;
	}
	wb->head = NULL;
	wb->current = NULL;
	wb->ptr = 0;
	wb->len = 0;
}

static void
rball_init(struct read_block * rb, char * buffer) {
	rb->buffer = buffer;
	rb->current = NULL;
	uint8_t header[4];
	memcpy(header,buffer,4);
	rb->len = header[0] | header[1] <<8 | header[2] << 16 | header[3] << 24;
	rb->ptr = 4;
	rb->len -= rb->ptr;
}

static int
rb_init(struct read_block *rb, struct block *b) {
	rb->buffer = NULL;
	rb->current = b;
	memcpy(&(rb->len),b->buffer,sizeof(rb->len));
	rb->ptr = sizeof(rb->len);
	rb->len -= rb->ptr;
	return rb->len;
}

static void *
rb_read(struct read_block *rb, void *buffer, int sz) {
	if (rb->len < sz) {
		return NULL;
	}

	if (rb->buffer) {
		int ptr = rb->ptr;
		rb->ptr += sz;
		rb->len -= sz;
		memcpy(buffer, rb->buffer + ptr, sz);
		return buffer;
	}

	if (rb->ptr == BLOCK_SIZE) {
		struct block * next = rb->current->next;
		free(rb->current);
		rb->current = next;
		rb->ptr = 0;
	}

	int copy = BLOCK_SIZE - rb->ptr;

	if (sz <= copy) {
		memcpy(buffer, rb->current->buffer + rb->ptr, sz);
		rb->ptr += sz;
		rb->len -= sz;
		return buffer;
	}

	char * tmp = buffer;

	memcpy(tmp, rb->current->buffer + rb->ptr, copy);
	sz -= copy;
	tmp += copy;
	rb->len -= copy;

	for (;;) {
		struct block * next = rb->current->next;
		free(rb->current);
		rb->current = next;

		if (sz < BLOCK_SIZE) {
			memcpy(tmp, rb->current->buffer, sz);
			rb->ptr = sz;
			rb->len -= sz;
			return buffer;
		}
		memcpy(tmp, rb->current->buffer, BLOCK_SIZE);
		sz -= BLOCK_SIZE;
		tmp += BLOCK_SIZE;
		rb->len -= BLOCK_SIZE;
	}
}

static void
rb_close(struct read_block *rb) {
	while (rb->current) {
		struct block * next = rb->current->next;
		free(rb->current);
		rb->current = next;
	}
	rb->len = 0;
	rb->ptr = 0;
}

static inline void
wb_nil(struct write_block *wb) {
	int n = TYPE_NIL;
	wb_push(wb, &n, 1);
}

static inline void
wb_boolean(struct write_block *wb, int boolean) {
	int n = COMBINE_TYPE(TYPE_BOOLEAN , boolean ? 1 : 0);
	wb_push(wb, &n, 1);
}

static inline void
wb_integer(struct write_block *wb, int v) {
	if (v == 0) {
		int n = COMBINE_TYPE(TYPE_NUMBER , 0);
		wb_push(wb, &n, 1);
	} else if (v<0) {
		int n = COMBINE_TYPE(TYPE_NUMBER , 4);
		wb_push(wb, &n, 1);
		wb_push(wb, &v, 4);
	} else if (v<0x100) {
		int n = COMBINE_TYPE(TYPE_NUMBER , 1);
		wb_push(wb, &n, 1);
		uint8_t byte = (uint8_t)v;
		wb_push(wb, &byte, 1);
	} else if (v<0x10000) {
		int n = COMBINE_TYPE(TYPE_NUMBER , 2);
		wb_push(wb, &n, 1);
		uint16_t word = (uint16_t)v;
		wb_push(wb, &word, 2);
	} else {
		int n = COMBINE_TYPE(TYPE_NUMBER , 4);
		wb_push(wb, &n, 1);
		wb_push(wb, &v, 4);
	}
}

static inline void
wb_number(struct write_block *wb, double v) {
	int n = COMBINE_TYPE(TYPE_NUMBER , 8);
	wb_push(wb, &n, 1);
	wb_push(wb, &v, 8);
}

static inline void
wb_pointer(struct write_block *wb, void *v) {
	int n = TYPE_USERDATA;
	wb_push(wb, &n, 1);
	wb_push(wb, &v, sizeof(v));
}

static inline void
wb_string(struct write_block *wb, const char *str, int len) {
	if (len < MAX_COOKIE) {
		int n = COMBINE_TYPE(TYPE_SHORT_STRING, len);
		wb_push(wb, &n, 1);
		if (len > 0) {
			wb_push(wb, str, len);
		}
	} else {
		int n;
		if (len < 0x10000) {
			n = COMBINE_TYPE(TYPE_LONG_STRING, 2);
			wb_push(wb, &n, 1);
			uint16_t x = (uint16_t) len;
			wb_push(wb, &x, 2);
		} else {
			n = COMBINE_TYPE(TYPE_LONG_STRING, 4);
			wb_push(wb, &n, 1);
			uint32_t x = (uint32_t) len;
			wb_push(wb, &x, 4);
		}
		wb_push(wb, str, len);
	}
}

static void _pack_one(lua_State *L, struct write_block *b, int index, int depth);

static int
wb_table_array(lua_State *L, struct write_block * wb, int index, int depth) {
	int array_size = lua_rawlen(L,index);
	if (array_size >= MAX_COOKIE-1) {
		int n = COMBINE_TYPE(TYPE_TABLE, MAX_COOKIE-1);
		wb_push(wb, &n, 1);
		wb_integer(wb, array_size);
	} else {
		int n = COMBINE_TYPE(TYPE_TABLE, array_size);
		wb_push(wb, &n, 1);
	}

	int i;
	for (i=1;i<=array_size;i++) {
		lua_rawgeti(L,index,i);
		_pack_one(L, wb, -1, depth);
		lua_pop(L,1);
	}

	return array_size;
}

static void
wb_table_hash(lua_State *L, struct write_block * wb, int index, int depth, int array_size) {
	lua_pushnil(L);
	while (lua_next(L, index) != 0) {
		if (lua_type(L,-2) == LUA_TNUMBER) {
			lua_Number k = lua_tonumber(L,-2);
			int32_t x = (int32_t)lua_tointeger(L,-2);
			if (k == (lua_Number)x && x>0 && x<=array_size) {
				lua_pop(L,1);
				continue;
			}
		}
		_pack_one(L,wb,-2,depth);
		_pack_one(L,wb,-1,depth);
		lua_pop(L, 1);
	}
	wb_nil(wb);
}

static void
wb_table(lua_State *L, struct write_block *wb, int index, int depth) {
	if (index < 0) {
		index = lua_gettop(L) + index + 1;
	}
	int array_size = wb_table_array(L, wb, index, depth);
	wb_table_hash(L, wb, index, depth, array_size);
}

static void
_pack_one(lua_State *L, struct write_block *b, int index, int depth) {
	if (depth > MAX_DEPTH) {
		wb_free(b);
		luaL_error(L, "serialize can't pack too depth table");
	}
	int type = lua_type(L,index);
	switch(type) {
	case LUA_TNIL:
		wb_nil(b);
		break;
	case LUA_TNUMBER: {
		int32_t x = (int32_t)lua_tointeger(L,index);
		lua_Number n = lua_tonumber(L,index);
		if ((lua_Number)x==n) {
			wb_integer(b, x);
		} else {
			wb_number(b,n);
		}
		break;
	}
	case LUA_TBOOLEAN:
		wb_boolean(b, lua_toboolean(L,index));
		break;
	case LUA_TSTRING: {
		size_t sz = 0;
		const char *str = lua_tolstring(L,index,&sz);
		wb_string(b, str, (int)sz);
		break;
	}
	case LUA_TLIGHTUSERDATA:
		wb_pointer(b, lua_touserdata(L,index));
		break;
	case LUA_TTABLE:
		wb_table(L, b, index, depth+1);
		break;
	default:
		wb_free(b);
		luaL_error(L, "Unsupport type %s to serialize", lua_typename(L, type));
	}
}

static void
_pack_from(lua_State *L, struct write_block *b, int from) {
	int n = lua_gettop(L) - from;
	int i;
	for (i=1;i<=n;i++) {
		_pack_one(L, b , from + i, 0);
	}
}

static int
_pack(lua_State *L) {
	struct write_block b;
	wb_init(&b, NULL);
	_pack_from(L,&b,0);
	struct block * ret = wb_close(&b);
	lua_pushlightuserdata(L,ret);
	return 1;
}

static int
_append(lua_State *L) {
	struct write_block b;
	wb_init(&b, lua_touserdata(L,1));
	_pack_from(L,&b,1);
	struct block * ret = wb_close(&b);
	lua_pushlightuserdata(L,ret);
	return 1;
}


static inline void
__invalid_stream(lua_State *L, struct read_block *rb, int line) {
	int len = rb->len;
	if (rb->buffer == NULL) {
		rb_close(rb);
	}
	luaL_error(L, "Invalid serialize stream %d (line:%d)", len, line);
}

#define _invalid_stream(L,rb) __invalid_stream(L,rb,__LINE__)

static double
_get_number(lua_State *L, struct read_block *rb, int cookie) {
	switch (cookie) {
	case 0:
		return 0;
	case 1: {
		uint8_t n = 0;
		uint8_t * pn = rb_read(rb,&n,1);
		if (pn == NULL)
			_invalid_stream(L,rb);
		return *pn;
	}
	case 2: {
		uint16_t n = 0;
		uint16_t * pn = rb_read(rb,&n,2);
		if (pn == NULL)
			_invalid_stream(L,rb);
		return *pn;
	}
	case 4: {
		int n = 0;
		int * pn = rb_read(rb,&n,4);
		if (pn == NULL)
			_invalid_stream(L,rb);
		return *pn;
	}
	case 8: {
		double n = 0;
		double * pn = rb_read(rb,&n,8);
		if (pn == NULL)
			_invalid_stream(L,rb);
		return *pn;
	}
	default:
		_invalid_stream(L,rb);
		return 0;
	}
}

static void *
_get_pointer(lua_State *L, struct read_block *rb) {
	void * userdata = 0;
	void ** v = (void **)rb_read(rb,&userdata,sizeof(userdata));
	if (v == NULL) {
		_invalid_stream(L,rb);
	}
	return *v;
}

static void
_get_buffer(lua_State *L, struct read_block *rb, int len) {
	char *tmp = (char *)malloc(len);
	char * p = rb_read(rb,tmp,len);
	if (p == NULL) {
		free(tmp);
		_invalid_stream(L,rb);
	}
	lua_pushlstring(L,p,len);
	free(tmp);
}

static void _unpack_one(lua_State *L, struct read_block *rb);

static void
_unpack_table(lua_State *L, struct read_block *rb, int array_size) {
	if (array_size == MAX_COOKIE-1) {
		uint8_t type = 0;
		uint8_t *t = rb_read(rb, &type, 1);
		if (t==NULL || (*t & 7) != TYPE_NUMBER) {
			_invalid_stream(L,rb);
		}
		array_size = (int)_get_number(L,rb,*t >> 3);
	}
	lua_createtable(L,array_size,0);
	int i;
	for (i=1;i<=array_size;i++) {
		_unpack_one(L,rb);
		lua_rawseti(L,-2,i);
	}
	for (;;) {
		_unpack_one(L,rb);
		if (lua_isnil(L,-1)) {
			lua_pop(L,1);
			return;
		}
		_unpack_one(L,rb);
		lua_rawset(L,-3);
	}
}

static void
_push_value(lua_State *L, struct read_block *rb, int type, int cookie) {
	switch(type) {
	case TYPE_NIL:
		lua_pushnil(L);
		break;
	case TYPE_BOOLEAN:
		lua_pushboolean(L,cookie);
		break;
	case TYPE_NUMBER:
		lua_pushnumber(L,_get_number(L,rb,cookie));
		break;
	case TYPE_USERDATA:
		lua_pushlightuserdata(L,_get_pointer(L,rb));
		break;
	case TYPE_SHORT_STRING:
		_get_buffer(L,rb,cookie);
		break;
	case TYPE_LONG_STRING: {
		uint32_t len;
		if (cookie == 2) {
			uint16_t *plen = rb_read(rb, &len, 2);
			if (plen == NULL) {
				_invalid_stream(L,rb);
			}
			_get_buffer(L,rb,(int)*plen);
		} else {
			if (cookie != 4) {
				_invalid_stream(L,rb);
			}
			uint32_t *plen = rb_read(rb, &len, 4);
			if (plen == NULL) {
				_invalid_stream(L,rb);
			}
			_get_buffer(L,rb,(int)*plen);
		}
		break;
	}
	case TYPE_TABLE: {
		_unpack_table(L,rb,cookie);
		break;
	}
	}
}

static void
_unpack_one(lua_State *L, struct read_block *rb) {
	uint8_t type = 0;
	uint8_t *t = rb_read(rb, &type, 1);
	if (t==NULL) {
		_invalid_stream(L, rb);
	}
	_push_value(L, rb, *t & 0x7, *t>>3);
}

static int
_unpack(lua_State *L) {
	struct block * blk = lua_touserdata(L,1);
	if (blk == NULL) {
		return luaL_error(L, "Need a block to unpack");
	}
	lua_settop(L,0);
	struct read_block rb;
	rb_init(&rb, blk);

	int i;
	for (i=0;;i++) {
		if (i%16==15) {
			lua_checkstack(L,i);
		}
		uint8_t type = 0;
		uint8_t *t = rb_read(&rb, &type, 1);
		if (t==NULL)
			break;
		_push_value(L, &rb, *t & 0x7, *t>>3);
	}

	rb_close(&rb);

	return lua_gettop(L);
}

static int
_print_mem(const char * buffer, int len, int size) {
	int i;
	for (i=0;i<len && i<size;i++) {
		printf("%02x ",(unsigned char)buffer[i]);
	}
	return size - len;
}

static int
_print(lua_State *L) {
	struct block *b = lua_touserdata(L,1);
	if (b==NULL) {
		return luaL_error(L, "dump null pointer");
	}
	int len = 0;
	memcpy(&len, b->buffer ,sizeof(len));
	len -= sizeof(len);
	printf("Len = %d\n",len);
	len = _print_mem(b->buffer + sizeof(len), BLOCK_SIZE - sizeof(len) , len);
	while (len > 0) {
		b=b->next;
		len = _print_mem(b->buffer, BLOCK_SIZE , len);
	}
	printf("\n");
	return 0;
}

static int
_serialize(lua_State *L) {
	struct block *b = lua_touserdata(L,1);
	struct read_block rb;
	rb_init(&rb, b);
	if (b==NULL) {
		return luaL_error(L, "dump null pointer");
	}

	uint32_t len = 0;
	memcpy(&len, b->buffer ,sizeof(len));

	uint8_t * buffer = malloc(len);
	uint8_t * ptr = buffer;
	int sz = len;
	while(len>0) {
		if (len >= BLOCK_SIZE) {
			memcpy(ptr, b->buffer, BLOCK_SIZE);
			ptr += BLOCK_SIZE;
			len -= BLOCK_SIZE;
		} else {
			memcpy(ptr, b->buffer, len);
			break;
		}
		b = b->next;
	}

	buffer[0] = sz & 0xff;
	buffer[1] = (sz>>8) & 0xff;
	buffer[2] = (sz>>16) & 0xff;
	buffer[3] = (sz>>24) & 0xff;

	rb_close(&rb);

	lua_pushlightuserdata(L, buffer);
	lua_pushinteger(L, sz);

	return 2;
}

static int
_deserialize(lua_State *L) {
	void * buffer = lua_touserdata(L,1);
	if (buffer == NULL) {
		return luaL_error(L, "deserialize null pointer");
	}

	lua_settop(L,0);
	struct read_block rb;
	rball_init(&rb, buffer);

	int i;
	for (i=0;;i++) {
		if (i%16==15) {
			lua_checkstack(L,i);
		}
		uint8_t type = 0;
		uint8_t *t = rb_read(&rb, &type, 1);
		if (t==NULL)
			break;
		_push_value(L, &rb, *t & 0x7, *t>>3);
	}

	free(buffer);

	return lua_gettop(L);
}

static int
_dump(lua_State *L) {
	const char *filename = luaL_checkstring(L, -1);
	lua_pop(L, 1);
	_pack(L);
	lua_replace(L, 1);
	if (_serialize(L) == 2) {
		void *buffer = lua_touserdata(L, -2);
		int sz = luaL_checkint(L, -1);
		FILE *fp = fopen(filename, "wb");
		if (fp) {
			fwrite(buffer, sz, 1, fp);
			fclose(fp);
			free(buffer);
		}

		lua_pushinteger(L, sz);

		return 2;
	}
	return 0;
}

int
_load(lua_State *L) {
	const char *filename = luaL_checkstring(L, 1);
	lua_remove(L, 1);
	FILE *fp = fopen(filename, "rb");
	if (fp) {
		fseek(fp, 0, SEEK_END);
		int sz = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		void *buffer = (void *)malloc(sz);
		if (sz != fread(buffer, 1, sz, fp)) {
			free(buffer);
			return 0;
		}
		fclose(fp);

		lua_pushlightuserdata(L, buffer);
		return _deserialize(L);
	}
}

int
luaopen_serialize(lua_State *L) {
	luaL_Reg l[] = {
		{ "pack", _pack },
		{ "unpack", _unpack },
		{ "append", _append },
		{ "print", _print },
		{ "serialize", _serialize },
		{ "deserialize", _deserialize },
		{ "dump", _dump },
		{ "load", _load },
		{ NULL, NULL },
	};
	luaL_newlib(L,l);
	return 1;
}
