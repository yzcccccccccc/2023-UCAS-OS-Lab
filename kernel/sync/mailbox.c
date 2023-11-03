#include <os/smp.h>
#include <os/sync.h>
#include <os/string.h>

mailbox_t mbox[MBOX_NUM];

void init_mbox(){
    for (int i = 0; i < MBOX_NUM; i++){
        mbox[i].allocated = 0;
        mbox[i].head = mbox[i].tail = 0;
        mbox[i].freespace = MAX_MBOX_LENGTH;
        do_mlock_init_ptr(&mbox[i].mlock);
        do_cond_init_ptr(&mbox[i].is_empty);
        do_cond_init_ptr(&mbox[i].is_full);
    }
}

int do_mbox_open(char *name){
    int mbox_idx = -1;
    // Check
    for (int i = 0; i < MBOX_NUM; i++){
        if (mbox[i].allocated && !strcmp(name, mbox[i].name)){
            mbox_idx = i;
            break;
        }
    }
    if (mbox_idx != -1)
        return mbox_idx;
    
    // Create
    for (int i = 0; i < MBOX_NUM; i++){
        if (!mbox[i].allocated){
            mbox[i].allocated = 1;
            strcpy(mbox[i].name, name);
            mbox_idx = i;
            break;
        }
    }
    return mbox_idx;
}

void do_mbox_close(int mbox_idx){
    mbox[mbox_idx].allocated = 0;
    mbox[mbox_idx].head = mbox[mbox_idx].tail = 0;
    mbox[mbox_idx].freespace = MAX_MBOX_LENGTH;

    // Release wait queue
    while (!list_empty(&mbox[mbox_idx].mlock.block_queue))
        do_mlock_release_ptr(&mbox[mbox_idx].mlock);
}

/*******************************************************
    Kinda like using a monitor ? :(
    WTF is the return value ......
********************************************************/

// Producer
int do_mbox_send(int mbox_idx, void *msg, int msg_length){
    do_mlock_acquire_ptr(&mbox[mbox_idx].mlock);
    while (mbox[mbox_idx].freespace < msg_length)
        do_cond_wait_ptr(&mbox[mbox_idx].is_full, &mbox[mbox_idx].mlock);
    for (int i = 0; i < msg_length; i++){
        mbox[mbox_idx].msg[mbox[mbox_idx].tail] = ((char *)msg)[i];
        mbox[mbox_idx].tail = (mbox[mbox_idx].tail + 1) % MAX_MBOX_LENGTH;
    }
    mbox[mbox_idx].freespace -= msg_length;
    do_cond_signal_ptr(&mbox[mbox_idx].is_empty);
    do_mlock_release_ptr(&mbox[mbox_idx].mlock);
    return 1;
}

// Consumer
int do_mbox_recv(int mbox_idx, void *msg, int msg_length){
    do_mlock_acquire_ptr(&mbox[mbox_idx].mlock);
    while (MAX_MBOX_LENGTH - mbox[mbox_idx].freespace < msg_length)
        do_cond_wait_ptr(&mbox[mbox_idx].is_empty, &mbox[mbox_idx].mlock);
    for (int i = 0; i < msg_length; i++){
        ((char *)msg)[i] = mbox[mbox_idx].msg[mbox[mbox_idx].head];
        mbox[mbox_idx].head = (mbox[mbox_idx].head + 1) % MAX_MBOX_LENGTH;
    }
    mbox[mbox_idx].freespace += msg_length;
    do_cond_signal_ptr(&mbox[mbox_idx].is_full);
    do_mlock_release_ptr(&mbox[mbox_idx].mlock);
    return 1;
}