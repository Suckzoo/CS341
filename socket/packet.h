#ifndef __PACKET_H__
#define __PACKET_H__
#include <stdint.h>
struct packet
{
	uint8_t op;
	uint8_t proto;
	uint16_t sum;
	uint32_t transid;
};
#endif
