#include <e1000.h>
#include <type.h>
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
    int rtv = e1000_transmit(txpacket, length);
    return rtv;  // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    // TODO: [p5-task2] Receive one network packet via e1000 device
    // TODO: [p5-task3] Call do_block when there is no packet on the way
    int rtv = 0;
    for (int i = 0, len; i < pkt_num; i++){
        retry:
            len = e1000_poll(rxbuffer);
            if (len == 0)
                goto retry;
        copyout((uint8_t *)(&len), (uint8_t *)(pkt_lens + i), sizeof(int));
        rxbuffer += len;
        rtv += len;
    }

    return rtv;  // Bytes it has received
}

void net_handle_irq(void)
{
    // TODO: [p5-task3] Handle interrupts from network device
}