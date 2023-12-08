#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define FILESIZE    32768
char file_content[FILESIZE];

const int print_location = 8;

uint16_t fletcher16(uint8_t *data, int n){
    uint16_t sum1 = 0;
    uint16_t sum2 = 0;

    for (int i = 0; i < n; i++){
        sum1 = (sum1 + data[i]) % 0xff;
        sum2 = (sum2 + sum1) % 0xff;
    }

    return (sum2 << 8) | sum1;
}

void show_content(uint8_t *data, int n){
    for (int i = 0; i < n; i++){
        printf("0x%02x ", data[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n");
}

int main(){
    sys_move_cursor(0, print_location);
    printf("[Net Test]: Start Test of sys_net_recv_stream test, receiving... :P\n");

    int filesz_bound = FILESIZE, filesz_target, filesz_recv = 0;
    sys_net_recv_stream(file_content, &filesz_bound);

    filesz_target = *(int *)file_content;
    filesz_recv += filesz_bound;

    sys_move_cursor(0, print_location);
    if (filesz_recv >= filesz_target){
        //show_content((uint8_t *)(file_content), 128);
        uint32_t fletcher_res = fletcher16((uint8_t *)(file_content + 4), filesz_target - 4);
        printf("[Net Test]: Succesesfully acquire the file (%d bytes), fletcher16: %d :D    \n", filesz_target, fletcher_res);
    }
    else
        printf("[Net Test]: Fail to acquire the file (recv/target: %d/%d) :(                                        \n", filesz_recv, filesz_target);
    return 0;
}