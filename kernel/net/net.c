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

uint16_t _uint16_rev(uint16_t val){
    return (val >> 8) | ((val & 0xff) << 8);
}

uint32_t _uint32_rev(uint32_t val){
    uint32_t byte1, byte2, byte3, byte4;
    byte1 = (val & 0xff000000) >> 24;
    byte2 = (val & 0x00ff0000) >> 16;
    byte3 = (val & 0x0000ff00) >> 8;
    byte4 = val & 0x000000ff;
    return byte1 | (byte2 << 8) | (byte3 << 16) | (byte4 << 24);
}

// [p5-task4]
char stream_pkt_buffer[STREAM_PKT_SIZE];
static uint32_t signal_pkt[256] = {
    0xffffffff, 0x5500ffff, 0xf77db57d, 0x00450008, 0x0000d400, 0x11ff0040,
    0xa8c073d8, 0x00e00101, 0xe914fb00, 0x0004e914, 0x0000,     0x005e0001,
    0x2300fb00, 0x84b7f28b, 0x00450008, 0x0000d400, 0x11ff0040, 0xa8c073d8,
    0x00e00101, 0xe914fb00, 0x0801e914, 0x0000};

tran_pkt_desc_t strm_pkt_desc[STREAM_PKT_NUM];
LIST_HEAD(strm_free_lst);
LIST_HEAD(strm_used_lst);
int max_bytes;

void stream_lst_init(){
    strm_free_lst.next = strm_free_lst.prev = &strm_free_lst;
    strm_used_lst.next = strm_used_lst.prev = &strm_used_lst;
    for (int i = 0; i < STREAM_PKT_NUM; i++){
        strm_pkt_desc[i].ack = 0;
        list_insert(&strm_free_lst, &strm_pkt_desc[i].list);
    }
}

int stream_lst_insert(int start_offset, int end_offset){
    // Get a free tran_pkt_desc
    tran_pkt_desc_t *pkt_ptr;

    // Find position
    list_node_t *lst_ptr = &strm_used_lst;
    while (lst_ptr->next != &strm_used_lst){
        pkt_ptr = (tran_pkt_desc_t *)((void *)lst_ptr->next - TRANPKT_LIST_OFFSET);
        if (pkt_ptr->start_offset == start_offset && pkt_ptr->end_offset == end_offset) // already exist
            return 0;
        if (pkt_ptr->start_offset >= end_offset)
            break;
        lst_ptr = lst_ptr->next;
    }
    
    printl("[NET][DAT] recv: (%d, %d)\n", start_offset, end_offset);

    // Get a free tran_pkt_desc
    list_node_t *inst_lst_ptr = list_pop(&strm_free_lst);
    assert(inst_lst_ptr != NULL);

    // Upload info
    pkt_ptr = (tran_pkt_desc_t *)((void *)inst_lst_ptr - TRANPKT_LIST_OFFSET);
    pkt_ptr->start_offset   = start_offset;
    pkt_ptr->end_offset     = end_offset;

    // Insert just after lst_ptr
    inst_lst_ptr->next  = lst_ptr->next;
    inst_lst_ptr->prev  = lst_ptr;
    lst_ptr->next->prev = inst_lst_ptr;
    lst_ptr->next       = inst_lst_ptr;

    return 1;
}

// return the length of the data part in the stream_pkt_buffer, copy the data to the buffer
int parse_stream_buffer(void *buffer){
    void *tran_buffer = stream_pkt_buffer + OSI_TRAN_BASE_OFFSET;

    // MAGIC
    uint8_t magic = *(uint8_t *)(tran_buffer + OSI_TRAN_MAGIC_OFFSET);
    if (magic != 0x45)
        return 0;
    assert(magic == 0x45);
    
    // FLAG
    uint8_t flag = *(uint8_t *)(tran_buffer + OSI_TRAN_FLAG_OFFSET);
    if (!(flag & OSI_TRAN_FLAG_DAT))
        return 0;
    assert(flag & OSI_TRAN_FLAG_DAT);

    // LEN
    uint16_t len = _uint16_rev(*(uint16_t *)(tran_buffer + OSI_TRAN_LEN_OFFSET));

    // SEQ
    uint32_t seq = _uint32_rev(*(uint32_t *)(tran_buffer + OSI_TRAN_SEQ_OFFSET));
    uint32_t end = seq + len > max_bytes ? max_bytes : seq + len;
    int suc = stream_lst_insert(seq, seq + len);

    if (suc){
        // DATA
        copyout((uint8_t *)(tran_buffer + OSI_TRAN_DATA_OFFSET), (uint8_t *)(buffer + seq), end - seq);
        return end - seq;
    }
    else
        return 0;
}

void do_signal_pkt_send(uint32_t seq, uint8_t flag){
    seq = _uint32_rev(seq);

    void *tran_send_buffer = (void *)signal_pkt + OSI_TRAN_BASE_OFFSET;
    // MAGIC
    *(uint8_t *)(tran_send_buffer + OSI_TRAN_MAGIC_OFFSET)  = OSI_MAGIC_MARK;
    // FLAG
    *(uint8_t *)(tran_send_buffer + OSI_TRAN_FLAG_OFFSET)   = flag;
    // LEN
    *(uint16_t *)(tran_send_buffer + OSI_TRAN_LEN_OFFSET)   = 0;
    // SEQ
    *(uint32_t *)(tran_send_buffer + OSI_TRAN_SEQ_OFFSET)   = seq;

    do_net_send((void *)signal_pkt, 64);
}

// 0: continue to recv pkts; 1: finish
int check_recv_list(){
    tran_pkt_desc_t *pkt_ptr, *pkt_nxt_ptr;
    list_node_t *lst_ptr = strm_used_lst.next;

    // Check
    while (lst_ptr != &strm_used_lst){
        pkt_ptr = (tran_pkt_desc_t *)((void *)lst_ptr - TRANPKT_LIST_OFFSET);
        if (!pkt_ptr->ack)
            break;
        lst_ptr = lst_ptr->next;
    }

    // everyone has been acknowledged :)
    if (lst_ptr == &strm_used_lst){
        lst_ptr = lst_ptr->prev;
        pkt_ptr = (tran_pkt_desc_t *)((void *)lst_ptr - TRANPKT_LIST_OFFSET);
        printl("[NET][RSD] rsd(last): %d\n", pkt_ptr->end_offset);
        do_signal_pkt_send(pkt_ptr->end_offset, OSI_TRAN_FLAG_RSD);
        return 1;
    }
    int ack_seq = -1, last = 0, bubble = 0;

    // First Pkt (Sequence 0)
    lst_ptr = strm_used_lst.next;
    pkt_ptr = (tran_pkt_desc_t *)((void *)lst_ptr - TRANPKT_LIST_OFFSET);
    if (pkt_ptr->start_offset != 0){
        ack_seq = 0;
        do_signal_pkt_send(0, OSI_TRAN_FLAG_RSD);
        printl("[NET][RSD] rsd: %d\n", 0);
    }

    // Resend for the missing pkts
    while (lst_ptr != &strm_used_lst){
        pkt_ptr = (tran_pkt_desc_t *)((void *)lst_ptr - TRANPKT_LIST_OFFSET);
        if (lst_ptr->next != &strm_used_lst){
            pkt_nxt_ptr = (tran_pkt_desc_t *)((void *)lst_ptr->next - TRANPKT_LIST_OFFSET);
            if (pkt_nxt_ptr->start_offset != pkt_ptr->end_offset){
                if (ack_seq == -1 && pkt_ptr->ack == 0 && bubble == 0)
                    ack_seq = pkt_ptr->end_offset;
                bubble = 1;
                printl("[NET][RSD] rsd: %d\n", pkt_ptr->end_offset);
                do_signal_pkt_send(pkt_ptr->end_offset, OSI_TRAN_FLAG_RSD);
            }
        }
        else{
            if (ack_seq == -1 && pkt_ptr->ack == 0 && bubble == 0){
                ack_seq = pkt_ptr->end_offset;
                last = 1;
            }
        }
        lst_ptr = lst_ptr->next;
    }

    // Ack the consecutive pkt begin with seq 0
    if (ack_seq != -1){
        // Mark the ack
        lst_ptr = strm_used_lst.next;
        while (lst_ptr != &strm_used_lst){
            pkt_ptr = (tran_pkt_desc_t *)((void *)lst_ptr - TRANPKT_LIST_OFFSET);
            if (pkt_ptr->start_offset < ack_seq)
                pkt_ptr->ack = 1;
            lst_ptr = lst_ptr->next;
        }
        
        // Send Ack
        if (ack_seq != 0){
            do_signal_pkt_send(ack_seq, OSI_TRAN_FLAG_ACK);
            printl("[NET][ACK] ack: %d\n", ack_seq);
            if (last){
                printl("[NET][RSD] rsd(last): %d\n", ack_seq);
                do_signal_pkt_send(ack_seq, OSI_TRAN_FLAG_RSD);
            }
        }
    }
    return 0;
}

void do_net_recv_stream(void *buffer, int *nbytes){
    int len, recv_bytes = 0, pkt_arrive;
    int recv_retry = 0, eof_retry = 0;
    copyin((uint8_t *)(&max_bytes), (uint8_t *)nbytes, sizeof(int));
    stream_lst_init();

work:
    // Recv
    pkt_arrive = 0;
    while ((len = e1000_poll(stream_pkt_buffer))){
        pkt_arrive = 1;
        recv_bytes += parse_stream_buffer(buffer);
    }

    // Wait?
    if (!pkt_arrive && recv_retry < RETRY_LIMIT){
        recv_retry++;
        do_sleep(1);
        goto work;
    }
    else{
        recv_retry = 0;
        // Check the recvs
        if (!check_recv_list()){
            do_sleep(1);
            goto work;
        }
        else{
            if (eof_retry < RETRY_LIMIT){
                eof_retry++;
                do_sleep(1);
                goto work;
            }
        }
    }

    // End
    copyout((uint8_t *)&recv_bytes, (uint8_t *)nbytes, sizeof(int));
    printl("\n");
}