#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <assert.h>
#include <pgtable.h>

#define LOW32_MASK  0xffffffff

// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
	/* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

	/* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

	/* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

	/**
     * Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
    latency(1);

	/* Clear interrupt mask to stop board from generating interrupts */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR)) ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* TODO: [p5-task1] Initialize tx descriptors */
    for (int i = 0; i < TXDESCS; i++){
        tx_desc_array[i].addr       = kva2pa((uint64_t)tx_pkt_buffer[i]);
        tx_desc_array[i].length     = 0;
        tx_desc_array[i].cso        = 0;
        tx_desc_array[i].cmd        = (~E1000_TXD_CMD_DEXT & (E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP));
        tx_desc_array[i].status     = 0;
        tx_desc_array[i].css        = 0;
        tx_desc_array[i].special    = 0;
    }

    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
    uint64_t tx_base    = kva2pa((uint64_t)tx_desc_array);
    e1000_write_reg(e1000, E1000_TDBAH, tx_base >> 32);
    e1000_write_reg(e1000, E1000_TDBAL, tx_base & LOW32_MASK);
    e1000_write_reg(e1000, E1000_TDLEN, TXDESCS * sizeof(struct e1000_tx_desc));

	/* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

    /* TODO: [p5-task1] Program the Transmit Control Register */
    uint32_t tctl_ct    = (0x10 << 4) & E1000_TCTL_CT;
    uint32_t tctl_cold  = (0x40 << 12) & E1000_TCTL_COLD;
    e1000_write_reg(e1000, E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | tctl_ct | tctl_cold);
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
    uint64_t mac_address    = 0;
    for (int i = 0; i < 6; i++)
        mac_address |= (enetaddr[i]) << (3 * i);
    e1000_write_reg_array(e1000, E1000_RA, 0, mac_address & LOW32_MASK);                // RAL[0]: 0x5400 + 8*idx
    e1000_write_reg_array(e1000, E1000_RA, 1, (mac_address >> 32) | E1000_RAH_AV);      // RAH[0]: 0x5404 + 8*idx

    /* TODO: [p5-task2] Initialize rx descriptors */
    for (int i = 0; i < RXDESCS; i++){
        rx_desc_array[i].addr       = kva2pa((uint64_t)rx_pkt_buffer[i]);
        rx_desc_array[i].length     = 0;
        rx_desc_array[i].csum       = 0;
        rx_desc_array[i].status     = 0;
        rx_desc_array[i].errors     = 0;
        rx_desc_array[i].special    = 0;
    }

    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
    uint64_t rx_base    = kva2pa((uint64_t)rx_desc_array);
    e1000_write_reg(e1000, E1000_RDBAL, rx_base & LOW32_MASK);
    e1000_write_reg(e1000, E1000_RDBAH, rx_base >> 32);
    e1000_write_reg(e1000, E1000_RDLEN, RXDESCS * sizeof(struct e1000_rx_desc));

    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, RXDESCS - 1);

    /* TODO: [p5-task2] Program the Receive Control Register */
    e1000_write_reg(e1000, E1000_RCTL, ~E1000_RCTL_BSEX & (E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SZ_2048));

    /* TODO: [p5-task3] Enable RXDMT0 Interrupt */
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length)
{
    /* TODO: [p5-task1] Transmit one packet from txpacket */
    local_flush_dcache();
    uint32_t head       = e1000_read_reg(e1000, E1000_TDH);
    uint32_t tail       = e1000_read_reg(e1000, E1000_TDT);
    uint32_t next_tail  = (tail + 1) % TXDESCS;

    if (head == next_tail){              // no space :(
        return 0;
    }

    // Step1: Content
    memcpy((uint8_t *)tx_pkt_buffer[tail], (uint8_t *)txpacket, length);

    // Step2: Descriptor
    tx_desc_array[tail].length  = length;
    tx_desc_array[tail].status  = 0;

    // Step3: update NIC reg
    e1000_write_reg(e1000, E1000_TDT, next_tail);
    local_flush_dcache();

    // Step3.1(task1): wait
retry:
    local_flush_dcache();
    if (tx_desc_array[tail].status == 0) 
        goto retry;
    

    // Step 4: finish
    return length;
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll(void *rxbuffer)
{
    /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */

    return 0;
}