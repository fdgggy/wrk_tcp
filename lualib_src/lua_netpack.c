
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define QUEUESIZE 1024
#define HASHSIZE 4096

#define TYPE_DATA 1
#define TYPE_MORE 2

struct netpack {
	int id;
	int size;
	void * buffer;
};

struct uncomplete {
	struct netpack pack;
	struct uncomplete * next;
	int read;
	int header;
};

struct queue {
	int cap;
	int head;
	int tail;
	struct uncomplete * hash[HASHSIZE];
	struct netpack queue[QUEUESIZE];
};

static const char *
tolstring(lua_State *L, size_t *sz, int index) {
	const char * ptr;
	if (lua_isuserdata(L,index)) {
		ptr = (const char *)lua_touserdata(L,index);
		*sz = (size_t)luaL_checkinteger(L, index+1);
	} else {
		ptr = luaL_checklstring(L, index, sz);
	}
	return ptr;
}

static inline void
write_size(uint8_t * buffer, int len) {
	buffer[0] = (len >> 8) & 0xff;
	buffer[1] = len & 0xff;
}

static int
lpackmsg(lua_State *L) {
	size_t len;
	const char * ptr = tolstring(L, &len, 5);
	if (len >= 0x10000) {
		return luaL_error(L, "Invalid size (too long) of data : %d", (int)len);
	}
	unsigned char * buffer = malloc(len + 13);
	write_size(buffer, len+11);
	int type = lua_tointeger(L, 1);
	buffer[2] = type & 0xff;
	int proto_id = lua_tointeger(L, 2);
	buffer[3] = (proto_id >> 8) & 0xff;
	buffer[4] = proto_id & 0xff;
	int client_id = lua_tointeger(L, 3);
	buffer[5] = (client_id >> 24) & 0xff;
	buffer[6] = (client_id >> 16) & 0xff;
	buffer[7] = (client_id >> 8) & 0xff;
	buffer[8] = client_id & 0xff;
	int session_id = lua_tointeger(L, 4);
	buffer[9] = (session_id >> 24) & 0xff;
	buffer[10] = (session_id >> 16) & 0xff;
	buffer[11] = (session_id >> 8) & 0xff;
	buffer[12] = session_id & 0xff;
	memmove(buffer+13, ptr, len);

	lua_pushlightuserdata(L, buffer);
	lua_pushinteger(L, len + 13);

	return 2;
}

static inline int
hash_fd(int fd) {
	int a = fd >> 24;
	int b = fd >> 12;
	int c = fd;
	return (int)(((uint32_t)(a + b + c)) % HASHSIZE);
}

static struct uncomplete *
find_uncomplete(struct queue *q, int fd) {
	if (q == NULL)
		return NULL;
	int h = hash_fd(fd);
	// printf("find_uncomplete fd:%d h:%d\n", fd, h);
	struct uncomplete * uc = q->hash[h];
	if (uc == NULL) {
		// printf("find_uncomplete uc is NULL 1\n");
		return NULL;
	}
	if (uc->pack.id == fd) {
		q->hash[h] = uc->next;
		return uc;
	}
	struct uncomplete * last = uc;
	while (last->next) {
		uc = last->next;
		if (uc->pack.id == fd) {
			last->next = uc->next;
			return uc;
		}
		last = uc;
	}
	// printf("find_uncomplete uc is NULL 2\n");

	return NULL;
}

static struct queue *
get_queue(lua_State *L) {
	struct queue *q = lua_touserdata(L,1);
	if (q == NULL) {
		q = lua_newuserdata(L, sizeof(struct queue));
		q->cap = QUEUESIZE;
		q->head = 0;
		q->tail = 0;
		int i;
		for (i=0;i<HASHSIZE;i++) {
			q->hash[i] = NULL;
		}
		lua_replace(L, 1);
	}
	return q;
}

static void
expand_queue(lua_State *L, struct queue *q) {
	struct queue *nq = lua_newuserdata(L, sizeof(struct queue) + q->cap * sizeof(struct netpack));
	nq->cap = q->cap + QUEUESIZE;
	nq->head = 0;
	nq->tail = q->cap;
	memcpy(nq->hash, q->hash, sizeof(nq->hash));
	memset(q->hash, 0, sizeof(q->hash));
	int i;
	for (i=0;i<q->cap;i++) {
		int idx = (q->head + i) % q->cap;
		nq->queue[i] = q->queue[idx];
	}
	q->head = q->tail = 0;
	lua_replace(L,1);
}

static void
push_data(lua_State *L, int fd, void *buffer, int size, int clone) {
	if (clone) {
		void * tmp = malloc(size);
		memcpy(tmp, buffer, size);
		buffer = tmp;
	}
	struct queue *q = get_queue(L);
	struct netpack *np = &q->queue[q->tail];
	// printf("push_data1 tail:%d\n", q->tail);
	if (++q->tail >= q->cap)
		q->tail -= q->cap;
	// printf("push_data2 tail:%d\n", q->tail);

	np->id = fd;
	np->buffer = buffer;
	np->size = size;
	// printf("push_data fd:%d size:%d head:%d tail:%d\n", fd, size, q->head, q->tail);
	if (q->head == q->tail) {
		expand_queue(L, q);
	}
}

static struct uncomplete *
save_uncomplete(lua_State *L, int fd) {
	struct queue *q = get_queue(L);
	int h = hash_fd(fd);
	// printf("save_uncomplete fd:%d h:%d\n", fd, h);
	struct uncomplete * uc = malloc(sizeof(struct uncomplete));
	memset(uc, 0, sizeof(*uc));
	uc->next = q->hash[h];
	uc->pack.id = fd;
	q->hash[h] = uc;

	return uc;
}

static inline int
read_size(uint8_t * buffer) {
	int r = (int)buffer[0] << 8 | (int)buffer[1];
	return r;
}

static void
push_more(lua_State *L, int fd, uint8_t *buffer, int size) {
	if (size == 1) {
		struct uncomplete * uc = save_uncomplete(L, fd);
		uc->read = -1;
		uc->header = *buffer;
		return;
	}
	int pack_size = read_size(buffer);
	buffer += 2;
	size -= 2;
	// printf("push_more pack_size:%d size:%d\n", pack_size, size);
	if (size < pack_size) {
		struct uncomplete * uc = save_uncomplete(L, fd);
		uc->read = size;
		uc->pack.size = pack_size;
		uc->pack.buffer = malloc(pack_size);
		memcpy(uc->pack.buffer, buffer, size);
		return;
	}
	push_data(L, fd, buffer, pack_size, 1);

	buffer += pack_size;
	size -= pack_size;
	if (size > 0) {
		push_more(L, fd, buffer, size);
	}
}

static inline int
filter_data (lua_State *L, unsigned char * buffer) {
	// printf("0000\n");
	struct queue *q = lua_touserdata(L,1);
	int size = luaL_checkinteger(L,3);
	int fd = luaL_checkinteger(L,4);
	// printf("size:%d, fd:%d\n", size, fd);
	lua_settop(L, 1); //栈里面只有queue
	if (q == NULL) {
		// printf("q is NULL\n");
	}
	struct uncomplete * uc = find_uncomplete(q, fd);
	if (uc) {
		// printf("uuuuu\n");
		// fill uncomplete
		if (uc->read < 0) { //包长前一个字节, read==-1
			// read size
			assert(uc->read == -1);
			int pack_size = *buffer;
			pack_size |= uc->header << 8 ;
			++buffer;
			--size;
			uc->pack.size = pack_size;
			uc->pack.buffer = malloc(pack_size);
			uc->read = 0;
		}
		int need = uc->pack.size - uc->read;
		// printf("p:%d r:%d need:%d\n", uc->pack.size, uc->read, need);

		if (size < need) {
			memcpy(uc->pack.buffer + uc->read, buffer, size);
			uc->read += size;
			int h = hash_fd(fd);
			uc->next = q->hash[h];
			q->hash[h] = uc;
			return 1;
		}
		memcpy(uc->pack.buffer + uc->read, buffer, need);
		buffer += need;
		size -= need;
		if (size == 0) {
			lua_pushvalue(L, lua_upvalueindex(TYPE_DATA));
			lua_pushinteger(L, fd);
			lua_pushlightuserdata(L, uc->pack.buffer);
			lua_pushinteger(L, uc->pack.size);
			free(uc);
			return 5;
		}
		// printf("more\n");
		// more data
		push_data(L, fd, uc->pack.buffer, uc->pack.size, 0);
		free(uc);
		push_more(L, fd, buffer, size);
		lua_pushvalue(L, lua_upvalueindex(TYPE_MORE));
		return 2;
	} else {
		if (size == 1) {
			struct uncomplete * uc = save_uncomplete(L, fd);
			uc->read = -1;
			uc->header = *buffer;
			return 1;
		}
		int pack_size = read_size(buffer);
		buffer+=2;
		size-=2;
		// printf("size:%d pack_size:%d\n", size, pack_size);

		if (size < pack_size) {
			struct uncomplete * uc = save_uncomplete(L, fd);
			uc->read = size;
			uc->pack.size = pack_size;
			uc->pack.buffer = malloc(pack_size);
			memcpy(uc->pack.buffer, buffer, size);
			return 1;
		}
		if (size == pack_size) {
			// just one package
			lua_pushvalue(L, lua_upvalueindex(TYPE_DATA));
			lua_pushinteger(L, fd);
			void * result = malloc(pack_size);
			memcpy(result, buffer, size);
			lua_pushlightuserdata(L, result);
			lua_pushinteger(L, size);
			return 5;
		}
		// more data
		push_data(L, fd, buffer, pack_size, 1);
		// printf("aaaa\n");
		buffer += pack_size;
		size -= pack_size;
		push_more(L, fd, buffer, size);
		// printf("bbb\n");

		lua_pushvalue(L, lua_upvalueindex(TYPE_MORE));
		// printf("ccc\n");

		return 2;
	}

    return 1;
}

static int
lfilter(lua_State *L) {
	unsigned char * buffer = lua_touserdata(L,2);
    int ret = filter_data(L, buffer);
    free(buffer);

    return ret;
}

static inline void
push_head(lua_State *L, unsigned char * msg) {
	lua_pushinteger(L, msg[0]);
	int proto_id = (int)(msg[2]);
	proto_id |= (int)(msg[1]) << 8;
	lua_pushinteger(L, proto_id);
	int client_id = (int)(msg[6]);
	client_id |= (int)(msg[5]) << 8;
	client_id |= (int)(msg[4]) << 16;
	client_id |= (int)(msg[3]) << 24;
	lua_pushinteger(L, client_id);
	int session_id = (int)(msg[10]);
	session_id |= (int)(msg[9]) << 8;
	session_id |= (int)(msg[8]) << 16;
	session_id |= (int)(msg[7]) << 24;
	lua_pushinteger(L, session_id);
}

static int
lunpackmsg(lua_State *L) {
	void * msg = lua_touserdata(L,1);
	int sz = luaL_checkinteger(L,2);
	if (msg == NULL || sz < 11) {
		return 0;
	} else {
		push_head(L, (unsigned char*)msg);
		lua_pushlstring(L, (const char *)(msg+11), sz-11);

		free(msg);

		return 5;
	}
}

static int
lpop(lua_State *L) {
	struct queue * q = lua_touserdata(L, 1);
	// printf("lpopppppppp head:%d tail:%d\n", q->head, q->tail);

	if (q == NULL || q->head == q->tail) {
		return 0;
	}
	struct netpack *np = &q->queue[q->head];
	if (++q->head >= q->cap) {
		q->head = 0;
	}
	// printf("lpop fd:%d size:%d delta:%d\n", np->id, np->size, q->head-q->tail);
	lua_pushinteger(L, np->id);
	lua_pushlightuserdata(L, np->buffer);
	lua_pushinteger(L, np->size);

	return 3;
}

int
luaopen_netpack(lua_State *L) {
	luaL_Reg l[] = {
		{ "pop", lpop },
		{ "packmsg", lpackmsg },
		{ "unpackmsg", lunpackmsg },

		{ NULL, NULL },
	};

	luaL_newlib(L, l);

    lua_pushliteral(L, "data");
	lua_pushliteral(L, "more");
	lua_pushcclosure(L, lfilter, 2);
	lua_setfield(L, -2, "filter");

	return 1;
}