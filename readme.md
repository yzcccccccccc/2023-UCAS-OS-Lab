# Project4-VirtualMemory
笔者实现了除Copy-On-Write之外的全部内容。  

## 一、物理页框管理  
使用以下结构管理物理页框：  
```C
typedef struct phy_pg{
    uint64_t    kva;                            // to some degrees, it's the physical frame addr
    uint64_t    va;                             // virtual frame addr
    list_node_t list;                           // link in used_queue (pinned & unpinned)
    list_node_t pcb_list;                       // pcb_list is used for the frame list in every pcb (may be of use when recycling)
    pcb_t       *user_pcb;
    pid_t       user_pid;
    uint64_t    attribute;
    pf_type_t   type;                           // pinned, unpinned, unused
}phy_pg_t;
extern phy_pg_t pf[NUM_MAX_PHYPAGE];
extern list_head free_pf, pinned_used_pf, unpinned_used_pf;
```  
`kva`记录该物理页框的物理地址，由于内核态上已将全部的物理空间做了映射，故直接使用kernel virtual address作为物理地址使用。  
`va`记录该物理页框对应的虚拟页框基地址（即4KB对齐）。  
其中物理页框有三种类型：钉住（pinned），活动（unpinned）以及未使用（unused）。在后续的换页操作中，我们只会换出活动页。  

## 二、交换区页框管理
如果说物理页框管理物理内存，那么交换去页框则是管理磁盘（SD卡）上的交换区。使用以下结构管理交换区页框：  
```C
// [p4] for managing the pages on the disk
typedef struct swp_pg{
    uint64_t    start_sector;                   // start_sector in swap area on the disk
    uint64_t    va;                             // virtual frame addr
    list_node_t list;                           // link in swp_queue
    list_node_t pcb_list;
    pcb_t       *user_pcb;
    pid_t       user_pid;
    uint64_t    attribute;
}swp_pg_t;
extern swp_pg_t sf[NUM_MAX_SWPPAGE];
extern list_head free_sf, used_sf;
extern uint64_t swap_start_offset, swap_start_sector;
```  
其中，全局变量`swap_start_offset`和`swap_start_sector`记录作为交换区的Sector的位置，用于初始化交换区页框。  

## 三、共享内存管理
使用以下结构管理共享内存：
```C
// [p4] for managing the shared memory pages
typedef enum{
    SHM_UNUSED,
    SHM_USED
}shm_stat_t;
typedef struct shm_pg{
    uint64_t kva;
    int key;
    int user_num;                               // how many users?
    shm_stat_t status;
    phy_pg_t *phy_page;
}shm_pg_t;
extern shm_pg_t shm_f[NUM_MAX_SHMPAGE];
#define SHM_PAGE_BASE   0x80000000                      // virtual addr of the base of shared memory pages
```  
目前共支持64个共享页框，起始虚地址为0x80000000，连续分配。在PCB控制块中增加一项`uint64_t shm_info;`，其64个比特位对应着64个共享虚地址的使用情况。注意，为进程分配共享页框会使shm_info发生改变，按需分配页时若遇到上述共享地址区域，也会给shm_info的对应位置1。  
共享页框对应的物理页归pid0_core0_pcb管理。共享页框分配的物理页为钉住页。  

## 四、换页
在调用`alloc_page_helper()`分配物理页后，会对应的分配一个交换区页，并将内容写到磁盘上。在换页时，若发现当前页是脏页，才需要将该物理页的内容写到交换区（事实上该功能笔者没有测试:laughing:，因为听说qemu和板卡在A/D位的行为不一致，于是偷懒在分配物理页时将D位置1了）。  
采用FIFO的方式选择换出页。  

## 五、安全页
在开启虚拟内存后，启动双核我们手搓的操作系统就进入了高频bug的状态，其中一种最经典的问题就是内核缺页。以下是一个笔者遇到的bug：  
* Core0换出了Core1的页
* Core1正在做系统调用，但是系统调用的数据存在该页上（特别是字符串等涉及指针类型的数据）。  
* Core0换完页愉快地退出内核态，此时Core1进行系统调用进入内核态，然而Core0已经把系统调用需要用的参数所在的页换出了，于是系统调用时缺页 :fearful:  

为解决这个问题，笔者在创建进程时同时为其创建一个安全页（Security Page），当用户态发生系统调用时，要求先把指针类参数指向的内容拷贝到该安全页上，同时将指针改指向安全页的对应位置，再进行系统调用。  
方案显然不是唯一的。事实上笔者当时还有以下备选方案：  
* 方案（1）：约定换页时不换出另一个核正在使用的页
* 方案（2）：系统调用进入内核态后，使用参数前先检查，若缺页则将对应页调入事先准备好的物理页框。  

当然，上述方案亦有其局限性（这也是笔者没有采用的原因）：  
* 方案（1）：若某核占用了大量的物理页，但是实际运行中使用不多，而这些页不会被换出，乐观的情况是卡顿，悲观的情况是无法换页导致系统完全卡死。  
* 方案（2）：若系统调用所需要的资源在多个页上，则会发生多次的页调入，影响效率。例如`*argv[]`，首先argv是一个指针，指向一块指针数组，内容可能在某页上；而指针数组的每一项又是指针，其指向的内容也可能在不同的页。  

## 六、多线程
首先明确一些（笔者设定）的定义：  
* 主（父）线程： 创建线程的进程，简单来说你可以认为进程就是主线程  
* 子线程： 被主线程创建的线程  

与之前的实验一致，笔者选择复用PCB作为线程控制单元。在PCB控制块中增加一项`struct pcb *par;`，用以记录当前线程的父线程。当前线程为主线程时，该指针为NULL。  
### 线程页表与内存  
子线程共用其父线程的页表。在内存管理上，子线程仅管理自己的线程栈，其他内存页（包括按需分配后获得的页）由父线程管理。所有子线程共享父线程的安全页，当然这就涉及到并发控制的问题，在分配安全页上的位置时笔者简单地采用互斥锁进行并发控制。  
### 线程创建与线程退出
线程的创建是简单的，只需将入口地址（即sepc的初始值）设置为给定的函数地址即可。真正需要考虑的是子线程退出。进程退出时（从main函数返回）会显式地调用sys_exit，这点由进程的“文件头”crt0.S控制；然而子线程完成给定的函数的运行后，却没有相应的控制方法。  
笔者的方法是在创建线程时，在安全页上划定一块区域插入一段“Magic Code”：  
```C
// In mm.h
    #define SECURITY_BOUND  0x5f00

// In pthread.c: pthread_create()
    // Step5: Magic Code!
    if (current_running[cpuid]->thread_type == MAIN_THREAD){            // create for the first time :)
        uint32_t *_code_location_ptr = (uint32_t *)SECURITY_BOUND;
        for (int i = 0; i < 7; i++, _code_location_ptr++){
            *_code_location_ptr = MAGIC_CODE[i];
        }
    }
```
然后初始化tcb时将ra的值设置为安全页上Magic Code的起始地址，这样子线程返回时就会自动到安全页上去执行这段代码，完成线程的退出和回收。至于这段“Magic Code”是什么，想必说到这大家也是心里有数了:smirk:。（心里没数的请自行将其翻译成汇编代码:triumph:）  
### 线程的Kill操作
当Kill的PID为主线程且其还有正在运行的子线程时，将其从ready_queue中剔除，回收除了内存页外的所有资源，将其状态设置为“ZOMBIE”；否则按正常杀死进程的流程执行。  
当Kill的PID为子线程时，回收子线程的所有资源，然后检查其父线程，若其状态为ZOMBIE且没有其他正在运行的子线程，则将其回收。  
需要注意的是ZOMBIE状态的父线程（即进程）仍会占有PCB块，这意味着如果系统有太多的僵尸，我们将无法启动新的进程。  

## 七、缺页
触发缺页时，首先检查当前进程使用的交换区页，若存在相应的页则为其分配一个物理页框，然后将内容拷贝到对应的物理内存中；若交换区上没有对应的页，则为其分配一个新的物理页。  
需要注意的是，若后者出现时是一个子线程触发的缺页异常，会将新分配的物理页挂载到子线程对应的父线程上。（还是那句话，子线程只负责管理自己的栈页:laughing:）  
此外，如果发生缺页的地址是在共享内存的范围，则会将进程对应的shm_info的对应位置1，再进行内存分配。这意味着之后请求共享内存是，该块用户虚拟地址不会再被分配。  

## 八、其他
* （1）支持爆栈的检测。但是还没有测试过。具体见`irq.c`处理缺页异常的部分。  
* （2）关于换页的测试（上点强度！）：笔者没有准备特别的测试程序，一般是直接限制可用的物理页框数（修改`mm.h`中的宏`NUM_MAX_PHYPAGE`）。  
* （3）在shell中新增`ms`命令，其作用是显示当前页框的使用情况（物理、交换区以及共享内存）。
* （4）启动双核后会出现各种各样的bug，包括但不限于pc飞到栈地址上、内核态缺页等。请务必仔细考虑以下情况：A. core0杀死core1上的进程。 B.core0换出core1正在使用的页。出现的问题大多的源自两个核使用独立的TLB和一级Cache。笔者采用核间中断的方式解决上述问题。  
* （5）何时将0x50200000 ~ 0x51000000段解映射？理想的情况是主核和从核都完成了初始化，即将打开中断开启抢占式调度的时候。笔者使用了一把自旋锁进行了同步的控制，主核在完成初始化后，等待从核完成解映射，再开启抢占式调度。  
*  （6）安全页的并发控制还是有问题。。。需要将测试程序mailbox的DATA_LENGTH改小才能通过测试。:sob: [2023.12.4]  

## 九、运行流程
略