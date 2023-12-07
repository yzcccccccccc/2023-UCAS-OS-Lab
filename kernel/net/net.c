#include <e1000.h>
#include <type.h>
#include <assert.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>
#include <os/net.h>

static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);

int do_net_send(void *txpacket, int length)
{
    // TODO: [p5-task1] Transmit one network packet via e1000 device
    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    // TODO: [p5-task3] Enable TXQE interrupt if transmit queue is full
    int rtv;
    int cpuid = get_current_cpu_id();

retry:
    rtv = e1000_transmit(txpacket, length);
    if (rtv == 0){
        printk("Panic: transmit fail\n");
        e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
        local_flush_dcache();
        do_block(&current_running[cpuid]->list, &send_block_queue);
        goto retry;
    }

    return rtv;  // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    // TODO: [p5-task2] Receive one network packet via e1000 device
    // TODO: [p5-task3] Call do_block when there is no packet on the way
    int cpuid = get_current_cpu_id();

    int rtv = 0;
    for (int i = 0, len; i < pkt_num; i++){
        retry:
            len = e1000_poll(rxbuffer);
            if (len == 0){
                printl("Panic: recv fail.\n");
                do_block(&current_running[cpuid]->list, &recv_block_queue);
                goto retry;
            }
        copyout((uint8_t *)(&len), (uint8_t *)(pkt_lens + i), sizeof(int));
        rxbuffer += len;
        rtv += len;
    }

    return rtv;  // Bytes it has received
}

void net_handle_irq(void)
{
    // TODO: [p5-task3] Handle interrupts from network device
    local_flush_dcache();
    uint32_t icr_val = e1000_read_reg(e1000, E1000_ICR);
    uint32_t ims_val = e1000_read_reg(e1000, E1000_IMS);
    icr_val &= ims_val;

    int handle_mark = 0;
    if (icr_val & E1000_ICR_TXQE){
        handle_mark = 1;

        list_node_t *list_ptr = list_pop(&send_block_queue);
        assert(list_ptr != NULL);

        do_unblock(list_ptr);

        if (list_empty(&send_block_queue)){
            e1000_write_reg(e1000, E1000_IMC, E1000_IMC_TXQE);
            local_flush_dcache();
        }
    }
    if (icr_val & E1000_ICR_RXDMT0){
        handle_mark = 1;

        list_node_t *list_ptr = list_pop(&recv_block_queue);
        //assert(list_ptr != NULL);

        if (list_ptr != NULL)
            do_unblock(list_ptr);
    }
    assert(handle_mark != 0);
}


// [p5-task4]
char stream_pkt_buffer[STREAM_PKT_SIZE];
char send_buffer[TX_PKT_SIZE];                      // for RSD & ACK

tran_pkt_desc_t stream_pkt_desc[STREAM_PKT_NUM];
tran_pkt_desc_t stream_list_head;
int pkt_cur, max_bytes;

void stream_lst_insert(int start_offset, int end_offset){
    stream_pkt_desc[pkt_cur].start_offset   = start_offset;
    stream_pkt_desc[pkt_cur].end_offset     = end_offset;

    tran_pkt_desc_t *pkt_ptr = &stream_list_head;
    while (pkt_ptr->next != &stream_list_head && pkt_ptr->next->start_offset >= end_offset)
        pkt_ptr = pkt_ptr->next;
    
    stream_pkt_desc[pkt_cur].next = pkt_ptr->next;
    stream_pkt_desc[pkt_cur].prev = pkt_ptr;
    pkt_ptr->next->prev = &stream_pkt_desc[pkt_cur];
    pkt_ptr->next = &stream_pkt_desc[pkt_cur];

    pkt_cur++;
}

// return the length of the data part in the stream_pkt_buffer, copy the data to the buffer
int parse_stream_buffer(void *buffer){
    void *tran_buffer = stream_pkt_buffer + OSI_TRAN_BASE_OFFSET;

    // MAGIC
    uint8_t magic = *(uint8_t *)(tran_buffer + OSI_TRAN_MAGIC_OFFSET);
    assert(magic == 0x45);
    
    // FLAG
    uint8_t flag = *(uint8_t *)(tran_buffer + OSI_TRAN_FLAG_OFFSET);
    assert(flag & OSI_TRAN_FLAG_DAT);

    // LEN
    uint16_t len = *(uint16_t *)(tran_buffer + OSI_TRAN_LEN_OFFSET);

    // SEQ
    uint32_t seq = *(uint32_t *)(tran_buffer + OSI_TRAN_SEQ_OFFSET);
    uint32_t end = seq + len > max_bytes ? max_bytes : seq + len;
    stream_lst_insert(seq, end);

    // DATA
    copyout((uint8_t *)(tran_buffer + OSI_TRAN_DATA_OFFSET), (uint8_t *)(buffer + seq), end - seq);


    return end - seq;
}

void do_signal_pkt_send(uint32_t seq, uint8_t flag){
    void *tran_send_buffer = send_buffer + OSI_TRAN_BASE_OFFSET;
    // MAGIC
    *(uint8_t *)(tran_send_buffer + OSI_TRAN_MAGIC_OFFSET)  = OSI_MAGIC_MARK;
    // FLAG
    *(uint8_t *)(tran_send_buffer + OSI_TRAN_FLAG_OFFSET)   = flag;
    // LEN
    *(uint16_t *)(tran_send_buffer + OSI_TRAN_LEN_OFFSET)   = 0;
    // SEQ
    *(uint32_t *)(tran_send_buffer + OSI_TRAN_SEQ_OFFSET)   = seq;

    do_net_send(send_buffer, 8);
}

// 0: continue to recv pkts; 1: finish
int check_recv_list(){
    tran_pkt_desc_t *pkt_ptr = stream_list_head.next;
    if (pkt_ptr == &stream_list_head)
        return 1;
    
    uint32_t ack_seq = -1;
    while (pkt_ptr != &stream_list_head){
        if (pkt_ptr->next != &stream_list_head && pkt_ptr->next->start_offset != pkt_ptr->end_offset){
            if (ack_seq == -1)  ack_seq = pkt_ptr->start_offset;
            do_signal_pkt_send(pkt_ptr->next->start_offset, OSI_TRAN_FLAG_RSD);
        }
        pkt_ptr = pkt_ptr->next;
    }
    if (ack_seq == -1)
        ack_seq = stream_list_head.prev->start_offset;

    do_signal_pkt_send(ack_seq, OSI_TRAN_FLAG_ACK);
    return 0;
}

void do_net_recv_stream(void *buffer, int *nbytes){
    int len, recv_bytes = 0;
    copyin((uint8_t *)(&max_bytes), (uint8_t *)nbytes, sizeof(int));

work:
    // Init the list
    pkt_cur = 0;
    stream_list_head.next = stream_list_head.prev = &stream_list_head;

    // Recv
    while ((len = e1000_poll(stream_pkt_buffer))) recv_bytes += parse_stream_buffer(buffer);

    // Check the recvs
    if (!check_recv_list())
        goto work;
    
    copyout((uint8_t *)&recv_bytes, (uint8_t *)nbytes, sizeof(int));
}