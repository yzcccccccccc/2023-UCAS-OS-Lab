# Project5-DeviceDriver
笔者完成了S,A,C Core的任务要求。  

## Virtual Memory的遗留问题  
* 安全页溢出  
在进入P5后，网卡收发包需要通过系统调用进行，这意味着包中的内容需要在内核和用户间来回穿梭。与之前实验中的小规模数据不同，网络包的规模可能更大（至少从数组大小来说是这样的 :confused: ），而我们的安全页只有4KB，很可能出现溢出。  
为此，笔者参考xv6增加了'safety-copy'的机制供内核使用，即边拷贝边检查缺页情况，详见`string.c`中的`copyin()`和`copyout()`。  
* Load Task时缺页  
当加载的程序过大时，可能出现在加载时就出现换页的操作。后续在“微调”位置时（具体见`loader.c`）可能缺页，而此时处于内核中，出现内核缺页。  
笔者尚未修复此bug:sob:。目前通过分配更多的物理页框暂时绕过该问题。  

## 网卡中断与时钟中断
* 网卡中断：发送中断TXQE  
当发送驱动函数`e1000_transmit()`提示未能发送成功时，打开TXQE并将当前进程阻塞到队列`send_block_queue`中；当TXQE到来时，从队列中唤醒一个进程，并将TXQE中断关闭。  
* 网卡中断：接收中断RXDMT0  
当接收驱动函数`e1000_poll()`提示当前接收队列已空时，将当前进程阻塞到队列`recv_block_queue`中；当RXDMT0到来时从队列中唤醒一个进程。初始化后，RXDMT0中断总是处于打开状态。  
* 时钟中断：  
支持每隔`CHECK_INTERVAL`个时钟中断进行发送和接收队列的检查。具体见`net.c`中的`net_timer_checker()`。  

## 可靠传输
### Function List
以下函数均在`net.c`中。  
* `do_net_recv_stream(void *buffer, int *nbytes)`：完整地接收一组包，并按序存放到指定缓冲区中。缓冲区大小为`*nbytes`，最后接收到的字节数也存放在`*nbytes`中。  
* `check_recv_list()`：检查已经接收的包，并完成ACK操作和RSD操作。  
* `do_signal_pkt_send(uint32_t seq, uint8_t flag)`：发包，即发送ACK包或者RSD包。  
* `parse_stream_buffer(void *buffer)`：处理一个接收到的包，即获得`flag`、`len`、`seq`等信息，并将包内容拷贝到`buffer`中的指定位置。  
* `stream_lst_insert(int start_offset, int end_offset)`：将接收到的包有序地组织到队列中（其实就是一个插入排序:yawning_face:）。  
* `stream_lst_init()`：初始化上一点所说的队列。  
### 接收流程
以下为`do_net_recv_stream()`的流程：  
* 0. 初始化  
* 1. 调用`e1000_poll()`，直到接收不到包，使用`parse_stream_buffer()`处理接收到的包  
* 2. 睡眠1s，转1。该步尝试`RETRY_LIMIT`次，到达次数后转3  
* 3. 清空2中的次数计数器。调用`check_recv_list()`检查接收到的包，若其中存在“空洞”（即需要RSD），则睡1s后转1；若不存在“空洞”，转4  
* 4. 睡眠1s，转1。该步尝试`RETRY_LIMIT`次，到达次数后结束`do_net_recv_stream()`函数  
(步骤4目的是解决最后一个包丢失的情况)  
### 测试程序
测试程序为`net_test.c`，其会根据文件内容和`do_net_recv_stream()`返回的接收字节数确定是否成功传输。接收成功会计算Fletcher16校验值，失败则会给出接收到的字节数和预期字节数。  

## 运行流程
**QEMU**
```bash
# 运行：
make run-net  
# 调试：
make debug-net
```  
笔者使用教学组提供的虚拟机环境。调试时若想要使用pktRxTx小程序，可在虚拟机中对应目录位置执行`sudo ./pktRxTx -m + 参数`，需要注意请务必按照小程序的说明完整配置环境。  
**板卡**
```bash
make minicom
# 之后使用loadbootm启动双核
```
上板时，笔者使用windows环境，通过命令提示符启动小程序。注意启动时需要有管理员权限。  