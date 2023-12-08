#include <os/string.h>
#include <os/mm.h>
#include <os/smp.h>
#include <assert.h>

void memcpy(uint8_t *dest, const uint8_t *src, uint32_t len)
{
    for (; len != 0; len--) {
        *dest++ = *src++;
    }
}

void memset(void *dest, uint8_t val, uint32_t len)
{
    uint8_t *dst = (uint8_t *)dest;

    for (; len != 0; len--) {
        *dst++ = val;
    }
}

void bzero(void *dest, uint32_t len)
{
    memset(dest, 0, len);
}

int strlen(const char *src)
{
    int i = 0;
    while (src[i] != '\0') {
        i++;
    }
    return i;
}

int strcmp(const char *str1, const char *str2)
{
    while (*str1 && *str2) {
        if (*str1 != *str2) {
            return (*str1) - (*str2);
        }
        ++str1;
        ++str2;
    }
    return (*str1) - (*str2);
}

int strncmp(const char *str1, const char *str2, int n)
{
    for (int i = 0; i < n; ++i)
    {
        if (str1[i] != str2[i])
        {
            return str1[i] - str2[i];
        }
    }

    return 0;
}

char *strcpy(char *dest, const char *src)
{
    char *tmp = dest;

    while (*src) {
        *dest++ = *src++;
    }

    *dest = '\0';

    return tmp;
}

char *strncpy(char *dest, const char *src, int n)
{
    char *tmp = dest;

    while (*src && n-- > 0) {
        *dest++ = *src++;
    }

    while (n-- > 0) {
        *dest++ = '\0';
    }

    return tmp;
}

char *strcat(char *dest, const char *src)
{
    char *tmp = dest;

    while (*dest != '\0') {
        dest++;
    }
    while (*src) {
        *dest++ = *src++;
    }

    *dest = '\0';

    return tmp;
}

//-------------------------------[p4] Safety copy (credited to xv6)-------------------------------

// copyin, usr_src is user virtual address
void copyin(uint8_t *ker_dst, uint8_t *usr_src, uint32_t len){
    int cpuid = get_current_cpu_id();
    uint64_t src_uva = ROUNDDOWN(usr_src, NORMAL_PAGE_SIZE);               // alignment to 4KB
    uint64_t src_kva;
    uint64_t pgdir = current_running[cpuid]->pgdir;
    for (int tmp_len = 0; len; len -= tmp_len){
        src_kva = get_kva_v(src_uva, pgdir);
        if (tmp_len == 0){
            int pgrm_len = src_uva + NORMAL_PAGE_SIZE - (uint64_t)usr_src;
            tmp_len = len > pgrm_len ? pgrm_len : len;
        }
        else
            tmp_len = len > NORMAL_PAGE_SIZE ? NORMAL_PAGE_SIZE : len;

        if (!src_kva){              // has been swapped out (kernel page fault)
            swp_pg_t *swp_ptr = query_swp_page(src_uva, current_running[cpuid]);
            assert(swp_ptr != NULL);
            swap_in(swp_ptr);
        }

        // copy in!
        for (int i = 0; i < tmp_len; i++){
            *(ker_dst) = *(usr_src);
            ker_dst++;
            usr_src++;
        }
        
        src_uva += NORMAL_PAGE_SIZE;
    }
}

// copyout, usr_src is user virtual address
void copyout(uint8_t *ker_src, uint8_t *usr_dst, uint32_t len){
    int cpuid = get_current_cpu_id();
    uint64_t dst_uva = ROUNDDOWN(usr_dst, NORMAL_PAGE_SIZE);
    uint64_t dst_kva;
    uint64_t pgdir = current_running[cpuid]->pgdir;
    for (int tmp_len = 0; len; len -= tmp_len){
        dst_kva = get_kva_v(dst_uva, pgdir);
        if (tmp_len == 0){
            int pgrm_len = dst_uva + NORMAL_PAGE_SIZE - (uint64_t)usr_dst;
            tmp_len = len > pgrm_len ? pgrm_len : len;
        }
        else
            tmp_len = len > NORMAL_PAGE_SIZE ? NORMAL_PAGE_SIZE : len;

        if (!dst_kva){
            swp_pg_t *swp_ptr = query_swp_page(dst_uva, current_running[cpuid]);
            assert(swp_ptr != NULL);
            swap_in(swp_ptr);
        }

        // copy out!
        for (int i = 0; i < tmp_len; i++){
            *(usr_dst) = *(ker_src);
            usr_dst++;
            ker_src++;
        }

        dst_uva += NORMAL_PAGE_SIZE;
    }
}