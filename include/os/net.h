#ifndef __INCLUDE_NET_H__
#define __INCLUDE_NET_H__

#include <os/list.h>
#include <type.h>

#define PKT_NUM 32

void net_handle_irq(void);
int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens);
int do_net_send(void *txpacket, int length);
void do_net_recv_stream(void *buffer, int *nbytes);

// Simplified OSI Transfer Layer
#define WINDOW_SIZE             10000
#define OSI_MAGIC_MARK          0x45
#define OSI_TRAN_BASE_OFFSET    54
#define OSI_TRAN_MAGIC_OFFSET   0
#define OSI_TRAN_FLAG_OFFSET    1
#define OSI_TRAN_LEN_OFFSET     2
#define OSI_TRAN_SEQ_OFFSET     4
#define OSI_TRAN_DATA_OFFSET    8

#define OSI_TRAN_FLAG_DAT       0x1
#define OSI_TRAN_FLAG_RSD       0x2
#define OSI_TRAN_FLAG_ACK       0x4

#define STREAM_PKT_SIZE         2048      
#define STREAM_PKT_NUM          64

#define TRANPKT_LIST_OFFSET     8
typedef struct tran_pkt_desc{
    int start_offset;
    int end_offset;
    list_node_t list;
    int ack;
}tran_pkt_desc_t;

extern uint16_t _uint16_rev(uint16_t val);
extern uint32_t _uint32_rev(uint32_t val);

#define RETRY_LIMIT 2

#endif  // !__INCLUDE_NET_H__