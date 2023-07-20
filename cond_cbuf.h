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

// to keep code analyzers happy
#ifndef ITEM_TYPE
typedef unsigned long ItemTypeDefault;
#define ITEM_TYPE ItemTypeDefault
#define TYPED_NAME(name) name##_ItemTypeDefault 
#endif

#include "cbuf.h"

typedef struct {
    TYPED_NAME(Cbuf) cbuf;
    pthread_cond_t fill_cond;
    pthread_cond_t space_cond;
} TYPED_NAME(CondCbuf);

static void TYPED_NAME(condcbuf_init)(TYPED_NAME(CondCbuf) *condcbuf,  
                                      ITEM_TYPE *buf, 
                                      size_t size)
{
    TYPED_NAME(cbuf_init)(&condcbuf->cbuf, buf, size);
    pthread_cond_init(&condcbuf->fill_cond, NULL);
    pthread_cond_init(&condcbuf->space_cond, NULL);
}

static void TYPED_NAME(condcbuf_deinit)(TYPED_NAME(CondCbuf) *condcbuf)
{
    pthread_cond_destroy(&condcbuf->fill_cond);
    pthread_cond_destroy(&condcbuf->space_cond);

    TYPED_NAME(cbuf_deinit)(&condcbuf->cbuf);
}

static void TYPED_NAME(condcbuf_write)(TYPED_NAME(CondCbuf) *condcbuf, 
                                       ITEM_TYPE *const items, 
                                       size_t cnt)
{
    size_t written = 0;
    while (written < cnt) {
        pthread_mutex_lock(&condcbuf->cbuf.wlock);

        size_t last_written;
        while ((last_written = TYPED_NAME(cbuf_write_locked)(&condcbuf->cbuf, 
                                                 items + written, 
                                                 cnt - written)) == 0) {
            pthread_cond_wait(&condcbuf->space_cond, &condcbuf->cbuf.wlock);
        }
        written += last_written;

        pthread_mutex_unlock(&condcbuf->cbuf.wlock);

        pthread_mutex_lock(&condcbuf->cbuf.rlock);
        pthread_cond_broadcast(&condcbuf->fill_cond);
        pthread_mutex_unlock(&condcbuf->cbuf.rlock);
    }
}

static void TYPED_NAME(condcbuf_read)(TYPED_NAME(CondCbuf) *condcbuf, 
                                      ITEM_TYPE *items, 
                                      size_t cnt)
{
    size_t read = 0;
    while (read < cnt) {
        pthread_mutex_lock(&condcbuf->cbuf.rlock); 

        size_t last_read; 
        while ((last_read = TYPED_NAME(cbuf_read_locked)(&condcbuf->cbuf, 
                                             items + read, 
                                             cnt - read)) == 0) {
            pthread_cond_wait(&condcbuf->fill_cond, &condcbuf->cbuf.rlock);
        }
        read += last_read;

        pthread_mutex_unlock(&condcbuf->cbuf.rlock);

        pthread_mutex_lock(&condcbuf->cbuf.wlock);
        pthread_cond_broadcast(&condcbuf->space_cond);
        pthread_mutex_unlock(&condcbuf->cbuf.wlock);
    }
}
