
#ifndef CHIPBEE_BUF_H
#define CHIPBEE_BUF_H

#define BUF_SIZE        512


struct chipbee_buf {
	int head;
	int tail;
	char data[BUF_SIZE];
};


static inline int chipbee_buf_available(struct chipbee_buf *buf)
{
	return buf->head - buf->tail;
}

static inline void chipbee_buf_drop(struct chipbee_buf *buf, int len)
{
	buf->tail += len;
	if (buf->tail >= buf->head) {
		buf->head = 0;
		buf->tail = 0;
	}
}

static inline int chipbee_buf_push(struct chipbee_buf *buf, const char *data, int len)
{
	if (len > BUF_SIZE - buf->head)
		len = BUF_SIZE - buf->head;
	memcpy(&buf->data[buf->head], data, len);
	buf->head += len;
	return len;
}

static inline int chipbee_buf_pop(struct chipbee_buf *buf, char *data, int len)
{
	if (len > buf->head - buf->tail)
		len = buf->head - buf->tail;
	memcpy(data, &buf->data[buf->tail], len);
	chipbee_buf_drop(buf, len);
	return len;
}

static inline int chipbee_buf_copy_from_user(struct chipbee_buf *buf, const char *user_buffer, int len)
{
	int result;

	if (len > BUF_SIZE - buf->head)
		len = BUF_SIZE - buf->head;
	result = copy_from_user(&buf->data[buf->head], user_buffer, len);
	if (result < 0)
		return result;
	buf->head += len;
	return len;
}

static inline int chipbee_buf_copy_to_user(struct chipbee_buf *buf, char *user_buffer, int len)
{
	int result;

	if (len > buf->head - buf->tail)
		len = buf->head - buf->tail;
	result = copy_to_user(user_buffer, &buf->data[buf->tail], len);
	if (result < 0)
		return result;
	chipbee_buf_drop(buf, len);
	return len;
}

#endif

