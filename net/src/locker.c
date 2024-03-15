//
// Created by xsy on 2024/3/15.
//

#include "locker.h"


net_err_t locker_init(locker_t * locker, locker_type_t type)
{
    locker->type = type;
    if (type == LOCKER_THREAD)
    {
        sys_mutex_t mutex = sys_mutex_create();
        if (mutex == SYS_MUTEX_INVALID)
        {
            return NET_ERR_SYS;
        }

        locker->mutex = mutex;
    }

    return NET_ERR_OK;
}

void locker_destroy(locker_t * locker)
{
    if (locker->type == LOCKER_THREAD)
    {
        sys_mutex_free(locker->mutex);
    }
}

void locker_lock(locker_t * locker)
{
    if (locker->type == LOCKER_THREAD)
    {
        sys_mutex_lock(locker->mutex);
    }
}

void locker_unlock(locker_t * locker)
{
    if (locker->type == LOCKER_THREAD)
    {
        sys_mutex_unlock(locker->mutex);
    }
}
