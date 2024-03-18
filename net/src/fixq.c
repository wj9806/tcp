//
// Created by wj on 2024/3/18.
//
#include "fixq.h"
#include "debug.h"

net_err_t fixq_init(fixq_t * q, void ** buf, int size, locker_type_t type)
{
    q->size = size;
    q->in = q->out = q->cnt = 0;
    q->buf = buf;
    q->send_sem = SYS_SEM_INVALID;
    q->recv_sem = SYS_SEM_INVALID;

    net_err_t err = locker_init(&q->locker, type);
    if (err < 0)
    {
        debug_error(DEBUG_QUEUE, "init locker failed");
        return err;
    }
    q->send_sem = sys_sem_create(q->size);
    if (q->send_sem == SYS_SEM_INVALID)
    {
        debug_error(DEBUG_QUEUE, "init send_sem failed");
        err = NET_ERR_SYS;
        goto init_failed;
    }
    q->recv_sem = sys_sem_create(0);
    if (q->recv_sem == SYS_SEM_INVALID)
    {
        debug_error(DEBUG_QUEUE, "init recv_sem failed");
        err = NET_ERR_SYS;
        goto init_failed;
    }
    q->buf = buf;
    return err;

    init_failed:
    if (q->send_sem != SYS_SEM_INVALID)
    {
        sys_sem_free(q->send_sem);
    }
    if (q->recv_sem != SYS_SEM_INVALID)
    {
        sys_sem_free(q->recv_sem);
    }
    locker_destroy(&q->locker);
    return err;
}

net_err_t fixq_send(fixq_t * q, void * msg, int ms)
{
    locker_lock(&q->locker);
    if ((ms < 0) && (q->cnt >= q->size))
    {
        locker_unlock(&q->locker);
        return NET_ERR_FULL;
    }
    locker_unlock(&q->locker);

    if (sys_sem_wait(q->send_sem, ms) < 0)
    {
        return NET_ERR_TMO;
    }

    locker_lock(&q->locker);
    q->buf[q->in++] = msg;
    if (q->in >= q->size)
    {
        q->in = 0;
    }
    q->cnt++;
    locker_unlock(&q->locker);

    sys_sem_notify(q->recv_sem);
    return NET_ERR_OK;
}

void * fixq_recv(fixq_t * q, int ms)
{
    locker_lock(&q->locker);
    if (!q->cnt && ms < 0)
    {
        locker_unlock(&q->locker);
        return (void *)0;
    }
    locker_unlock(&q->locker);
    if (sys_sem_wait(q->recv_sem, ms) < 0)
    {
        return (void *)0;
    }
    locker_lock(&q->locker);
    void * msg = q->buf[q->out++];
    if (q->out >= q->size)
    {
        q->out = 0;
    }
    q->cnt--;
    locker_unlock(&q->locker);

    sys_sem_notify(q->send_sem);
    return msg;
}

void fixq_destroy(fixq_t * q)
{
    locker_destroy(&q->locker);
    sys_sem_free(q->send_sem);
    sys_sem_free(q->recv_sem);
}

int fixq_count(fixq_t * q)
{
    locker_lock(&q->locker);
    int cnt = q->cnt;
    locker_unlock(&q->locker);
    return cnt;
}
