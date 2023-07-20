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
#include "cbuf_src.h"

void TYPED_NAME(semcbuf_init)(TYPED_NAME(SemCbuf) *cbuf, ITEM_TYPE *buf, size_t size)
{
    TYPED_NAME(cbuf_init)(&cbuf->cbuf, buf, size);
    sem_init(&cbuf->fill, 0, 0);
    sem_init(&cbuf->space, 0, size);
}

void TYPED_NAME(semcbuf_deinit)(TYPED_NAME(SemCbuf) *cbuf)
{
    sem_destroy(&cbuf->fill);
    sem_destroy(&cbuf->space);

    TYPED_NAME(cbuf_deinit)(&cbuf->cbuf);
}

void TYPED_NAME(semcbuf_push)(TYPED_NAME(SemCbuf) *cbuf, ITEM_TYPE *item)
{
    sem_wait(&cbuf->space);

    pthread_mutex_lock(&cbuf->cbuf.wlock);
    size_t written = TYPED_NAME(cbuf_write_locked)(&cbuf->cbuf, item, 1);
    assert(written == 1);
    pthread_mutex_unlock(&cbuf->cbuf.wlock);
    
    sem_post(&cbuf->fill);
}

void TYPED_NAME(semcbuf_pop)(TYPED_NAME(SemCbuf) *cbuf, ITEM_TYPE *item)
{
    sem_wait(&cbuf->fill);

    pthread_mutex_lock(&cbuf->cbuf.rlock);
    size_t read = TYPED_NAME(cbuf_read_locked)(&cbuf->cbuf, item, 1);
    assert(read == 1);
    pthread_mutex_unlock(&cbuf->cbuf.rlock);

    sem_post(&cbuf->space);
}
