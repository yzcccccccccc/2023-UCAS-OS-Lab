#include <e1000.h>
#include <type.h>
#include <assert.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>

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
        printl("Panic: transmit fail\n");
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