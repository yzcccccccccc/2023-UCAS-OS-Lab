#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define FILESIZE    65536
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

int main(){
    sys_move_cursor(0, print_location);
    printf("[Net Test]: Start Test of sys_net_recv_stream....\n");

    int filesz_bound = FILESIZE;
    sys_net_recv_stream(file_content, &filesz_bound);
    sys_move_cursor(0, print_location);
    printf("[Net Test]: Succesesfully acquired the file :)\n");
    return 0;
}