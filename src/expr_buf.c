#include <string.h>
#include "expr_buf.h"

struct expr_buf expr_buf_new() {
	return (struct expr_buf) {
		.data = NULL,
		.capacity = 0,
		.size = 0,
		.off = 0
	};
}

void expr_buf_extend_front(struct expr_buf * buf, size_t n) {
	if (buf->off >= n) {
		buf->off -= n;
		return;
	}
	size_t diff = n - buf->off;
	if (buf->size + diff > buf->capacity) {
		if (buf->capacity < 16) buf->capacity = 16;
		while (buf->size + diff > buf->capacity) {
			buf->capacity *= 2;
		}
		buf->data = realloc(buf->data, sizeof(struct expr) * buf->capacity);
	}
	memmove(buf->data + n, buf->data + buf->off, sizeof(struct expr) * (buf->size - buf->off));
	buf->off = 0;
	buf->size += diff;
}

// owned e
void expr_buf_push(struct expr_buf * buf, struct expr e) {
	if (buf->size >= buf->capacity) {
		if (buf->off > 0) {
			buf->size -= buf->off;
			memmove(buf->data, buf->data + buf->off, sizeof(struct expr) * buf->size);
		} else {
			size_t new_capacity = buf->capacity * 2;
			if (new_capacity < 16) new_capacity = 16;
			buf->data = realloc(buf->data, sizeof(struct expr) * new_capacity);
			buf->capacity = new_capacity;
		}
	}
	buf->data[buf->size++] = e;
}
