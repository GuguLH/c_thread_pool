#include "thread_pool.h"
#include "debug.h"

thread_pool_t *thread_pool_create(const int n_min, const int n_max, const int q_size)
{
    int i = 0, ret = 0;
    // 1 初始化thread_pool_t
    thread_pool_t *pool = NULL;
    pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));
    if (pool == NULL)
    {
        DEBUG_INFO("[ERROR] malloc(): %s\n", strerror(errno));
        goto err;
    }

    // 2 初始化任务队列
    pool->task_queue = (task_t *)malloc(sizeof(task_t) * q_size);
    if (pool->task_queue == NULL)
    {
        DEBUG_INFO("[ERROR] malloc(): %s\n", strerror(errno));
        goto err;
    }
    pool->q_capacity = q_size;
    pool->q_size = 0;
    pool->q_front = 0;
    pool->q_rear = 0;

    // 3 初始化线程池相关参数
    pool->worker_ids = (pthread_t *)malloc(sizeof(pthread_t) * n_min);
    if (pool->worker_ids == NULL)
    {
        DEBUG_INFO("[ERROR] malloc(): %s\n", strerror(errno));
        goto err;
    }
    pool->min_num = n_min;
    pool->max_num = n_max;
    pool->busy_num = 0;
    pool->live_num = n_min;
    pool->die_num = 0;

    ret = pthread_create(&pool->manager_id, NULL, manager, NULL);
    if (ret != 0)
    {
        DEBUG_INFO("[ERROR] pthread_create()\n");
        goto err;
    }
    for (i = 0; i < pool->min_num; i++)
    {
        ret = pthread_create(pool->worker_ids[i], NULL, worker, NULL);
        if (ret != 0)
        {
            DEBUG_INFO("[ERROR] pthread_create()\n");
            goto err;
        }
    }

    // 4 初始化互斥锁和条件变量
    if (pthread_mutex_init(&pool->mtx_pool, NULL) != 0 || pthread_mutex_init(&pool->mtx_busy, NULL) != 0 || pthread_cond_init(&pool->cond_full, NULL) != 0 || pthread_cond_init(&pool->cond_empty, NULL) != 0)
    {
        DEBUG_INFO("[ERROR] pthread_mutex_init()\n");
        goto err;
    }

    // 5 初始化销毁标志
    pool->shutdown = 0;

    return pool;
err:
    if (pool && pool->worker_ids)
    {
        free(pool->worker_ids);
        pool->worker_ids = NULL;
    }
    if (pool && pool->task_queue)
    {
        free(pool->task_queue);
        pool->task_queue = NULL;
    }
    if (pool)
    {
        free(pool);
        pool = NULL;
    }
}