/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * Copyright (C) 2018 Institute of Computing
 * Technology, CAS Author : Han Shukai (email :
 * hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * Changelog: 2019-8 Reimplement queue.h.
 * Provide Linux-style doube-linked list instead of original
 * unextendable Queue implementation. Luming
 * Wang(wangluming@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * */

#ifndef INCLUDE_LIST_H_
#define INCLUDE_LIST_H_

#include <type.h>

// double-linked list
typedef struct list_node
{
    struct list_node *next, *prev;
} list_node_t;

typedef list_node_t list_head;

// LIST_HEAD is used to define the head of a list.
#define LIST_HEAD(name) struct list_node name = {&(name), &(name)}

/* TODO: [p2-task1] implement your own list API */
// int list_empty(head_ptr): return 1 if list is empty
static inline int list_empty(list_node_t *list_head_ptr){
    return list_head_ptr->next == list_head_ptr;
}

// void list_insert(head_ptr, node_ptr): insert node into head
static inline void list_insert(list_node_t *list_head_ptr, list_node_t *list_node_ptr){
    list_node_t *ptr = list_head_ptr;
    while (ptr->next != list_head_ptr) ptr = ptr->next;
    list_node_ptr->prev = ptr;
    ptr->next = list_node_ptr;
    list_node_ptr->next = list_head_ptr;
    list_head_ptr->prev = list_node_ptr;
    return;
}

// delete *ptr node
static inline void list_delete(list_node_t *ptr){
    if (list_empty(ptr))
        return;
    ptr->prev->next = ptr->next;
    ptr->next->prev = ptr->prev;
    ptr->prev = ptr->next = NULL;
}

// list_node_t *list_delete(head_ptr): pop node from head
static inline list_node_t *list_pop(list_node_t *list_head_ptr){
    if (list_empty(list_head_ptr))
        return NULL;
    list_node_t *ret = list_head_ptr->next;
    list_head_ptr->next = list_head_ptr->next->next;
    list_head_ptr->next->prev = list_head_ptr;
    ret->next = ret->prev = ret;
    return ret;
}

#endif
