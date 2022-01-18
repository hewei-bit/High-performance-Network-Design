#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <limits.h>  // for uint_max

#include <unistd.h>

typedef struct ringbuffer_s {
    char * buf;
    unsigned int size; // buffer 的大小
    unsigned int write_pos; // 下一个可写的位置
    unsigned int read_pos; // 下一个可读的位置
} ringbuffer_t;

#define min(lth, rth) ((lth)<(rth)?(lth):(rth))

static inline int is_power_of_two(unsigned int num) {
    if (num < 2) return 0;
    return (num & (num - 1)) == 0;
}

static inline unsigned int roundup_power_of_two(unsigned int num) {
    if (num == 0) return 2;
    int i = 0;
    for (; num != 0; i++)
        num >>= 1;
    return 1U << i;
}

void rb_init(ringbuffer_t *r, unsigned int sz) {
    if (!is_power_of_two(sz)) sz = roundup_power_of_two(sz);
    r->buf = (char *)malloc(sz * sizeof(char));
    r->size = sz;
    r->write_pos = 0;
    r->read_pos = 0;
}

unsigned int rb_isempty(ringbuffer_t *r) {
    return r->read_pos == r->write_pos;
}

unsigned int rb_isfull(ringbuffer_t *r) {
    return r->size == (r->write_pos - r->read_pos);
}

void rb_free(ringbuffer_t *r) {
    if (r->buf != 0) {
        free(r->buf);
        r->buf = 0;
    }
    r->read_pos = r->write_pos = r->size = 0;
}

void rb_clear(ringbuffer_t *r) {
    r->read_pos = r->write_pos = 0;
}

unsigned int rb_len(ringbuffer_t *r) {
    return r->write_pos - r->read_pos;
}

unsigned int rb_remain(ringbuffer_t *r) {
    return r->size- r->write_pos + r->read_pos;
}

unsigned int rb_write(ringbuffer_t *r, char* buf, unsigned int sz) {
    if (sz > rb_remain(r)) {
        // 剩余空间不足
        return 0;
    }

#ifdef USE_BARRIER
// 确保在开始移动buffer的时候，先采样好 read_pos
    smp_mb();
#endif
    unsigned int i;
    i = min(sz, r->size - (r->write_pos & (r->size - 1)));

    memcpy(r->buf + (r->write_pos & (r->size - 1)), buf, i);
    memcpy(r->buf, buf+i, sz-i);

#ifdef USE_BARRIER
// 确保移动数据在修改 write_pos 之前
    smp_wmb();
#endif

    r->write_pos += sz;
    return sz;
}

unsigned int rb_read(ringbuffer_t *r, char* buf, unsigned int sz) {
    if (rb_isempty(r)) {
        // buffer 为空
        return 0;
    }
    unsigned int i;
    sz = min(sz, r->write_pos - r->read_pos);

#ifdef USE_BARRIER
// 确保在开始移动buffer的时候，先采样好 write_pos
    smp_rmb();
#endif

    i = min(sz, r->size - (r->read_pos & (r->size - 1)));
    memcpy(buf, r->buf+(r->read_pos & (r->size - 1)), i);
    memcpy(buf+i, r->buf, sz-i);
    
#ifdef USE_BARRIER
// 确保移动数据在修改 read_pos 之前
    smp_mb();
#endif
    r->read_pos += sz;
    return sz;
}

unsigned int rb_readline(ringbuffer_t *r, char* buf, unsigned int sz) {
    if (rb_isempty(r)) {
        return 0;
    }

    for (int i = 0; i < rb_len(r) || i <= sz; i++) {
        int pos = (r->read_pos + i) & (r->size - 1); 
        buf[i] = r->buf[pos];
        if (r->buf[pos] == '\n') {
            r->read_pos += i+1;
            return i+1;
        }
    }

    return 0;
}

