/* This is free and unencumbered software released into the public domain.
 * 
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 * 
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * 
 * For more information, please refer to <https://unlicense.org>
 * */
#include "cbuf.h"

static void TYPED_NAME(cbuf_init)(TYPED_NAME(Cbuf) *cb, ITEM_TYPE *buf, size_t size)
{
    cb->buf = buf;
    cb->size = size;
    cb->mask = size - 1;
    atomic_store(&cb->ri, 0);
    atomic_store(&cb->wi, 0);
    pthread_mutex_init(&cb->rlock, NULL);
    pthread_mutex_init(&cb->wlock, NULL);
}

static void TYPED_NAME(cbuf_deinit)(TYPED_NAME(Cbuf) *cb)
{
    pthread_mutex_destroy(&cb->rlock);
    pthread_mutex_destroy(&cb->wlock);
}

static size_t TYPED_NAME(cbuf_fill)(TYPED_NAME(Cbuf) *cbuf, unsigned long *ri)
{
    *ri = atomic_load_explicit(&cbuf->ri, memory_order_relaxed);
    return atomic_load_explicit(&cbuf->wi, memory_order_acquire) - *ri; 
}

static size_t TYPED_NAME(cbuf_space)(TYPED_NAME(Cbuf) *cbuf, unsigned long *wi)
{
    *wi = atomic_load_explicit(&cbuf->wi, memory_order_relaxed);
    return cbuf->size - 
           (*wi - atomic_load_explicit(&cbuf->ri, memory_order_relaxed)); 
}

static size_t TYPED_NAME(cbuf_write_locked)(TYPED_NAME(Cbuf) *cbuf, ITEM_TYPE const *items, size_t cnt)
{
    unsigned long wi;
    size_t const space =  TYPED_NAME(cbuf_space)(cbuf, &wi);
    size_t const to_write =  space < cnt ? space : cnt;

    for (unsigned long i = 0; i < to_write; i++, wi++) {
        cbuf->buf[wi & cbuf->mask] = items[i];
    }

    atomic_store_explicit(&cbuf->wi, wi, memory_order_release);

    return to_write;
}

static size_t TYPED_NAME(cbuf_read_locked)(TYPED_NAME(Cbuf) *cbuf, ITEM_TYPE *items, size_t cnt)
{
    unsigned long ri;
    size_t const fill = TYPED_NAME(cbuf_fill)(cbuf, &ri);
    size_t const to_read = fill < cnt ? fill : cnt;

    for (unsigned long i = 0; i < to_read; i++, ri++) {
        items[i] = cbuf->buf[ri & cbuf->mask];
    }

    atomic_store_explicit(&cbuf->ri, ri, memory_order_release);

    return to_read;
}
