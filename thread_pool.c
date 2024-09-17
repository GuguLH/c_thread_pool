#include "thread_pool.h"
#include "debug.h"

void thread_exit(thread_pool_t *pool)
{
    pthread_t tid = pthread_self();
    int i = 0;
    for (i = 0; i < pool->max_num; i++)
    {
        if (pool->worker_ids[i] == tid)
        {
            pool->worker_ids[i] = 0;
            DEBUG_INFO("[INFO] Thread [%ld] exiting...\n", tid);
            break;
        }
    }
    pthread_exit(NULL);
}

void *manager(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *)arg;
    while (!pool->shutdown)
    {
        // 每隔3s检测一次
        sleep(3);

        // 取出线程池中的任务数量和当前线程的数量
        pthread_mutex_lock(&pool->mtx_pool);
        int q_size = pool->q_size;
        int live_num = pool->live_num;
        pthread_mutex_unlock(&pool->mtx_pool);

        // 取出忙的线程数量
        pthread_mutex_lock(&pool->mtx_busy);
        int busy_num = pool->busy_num;
        pthread_mutex_unlock(&pool->mtx_busy);

        // 添加规则:
        // 任务个数 > 存活的个数 && 存活的个数 < 最大线程数
        if (q_size > live_num && live_num < pool->max_num)
        {
            pthread_mutex_lock(&pool->mtx_pool);
            int counter = 0, i = 0;
            for (i = 0; i < pool->max_num && counter < OP_NUM && pool->live_num < pool->max_num; i++)
            {
                if (pool->worker_ids[i] == 0)
                {
                    pthread_create(&pool->worker_ids[i], NULL, worker, pool);
                    counter++;
                    pool->live_num++;
                }
            }
            pthread_mutex_unlock(&pool->mtx_pool);
        }

        // 销毁规则:
        // 繁忙个数 * 2 < 存活个数 && 存活个数 > 最小线程数
        if (busy_num * 2 < live_num && live_num > pool->min_num)
        {
            pthread_mutex_lock(&pool->mtx_pool);
            pool->die_num = OP_NUM;
            pthread_mutex_unlock(&pool->mtx_pool);

            // 让工作线程自杀
            int i = 0;
            for (i = 0; i < OP_NUM; i++)
            {
                pthread_cond_signal(&pool->cond_empty);
            }
        }
    }
}

void *worker(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *)arg;
    for (;;)
    {
        pthread_mutex_lock(&pool->mtx_pool);

        // 任务队列为空阻塞等待
        while (pool->q_size == 0 && !pool->shutdown)
        {
            pthread_cond_wait(&pool->cond_empty, &pool->mtx_pool);
            // 判断是不是要销毁线程
            if (pool->die_num > 0)
            {
                pool->die_num--;
                if (pool->live_num > pool->min_num)
                {
                    pool->live_num--;
                    pthread_mutex_unlock(&pool->mtx_pool);
                    thread_exit(pool);
                }
            }
        }

        if (pool->shutdown)
        {
            pthread_mutex_unlock(&pool->mtx_pool);
            thread_exit(pool);
        }

        task_t task;
        task.function = pool->task_queue[pool->q_front].function;
        task.arg = pool->task_queue[pool->q_front].arg;
        // 更新队头索引
        pool->q_front = (pool->q_front + 1) % pool->q_capacity;
        pool->q_size--;

        pthread_cond_signal(&pool->cond_full);
        pthread_mutex_unlock(&pool->mtx_pool);

        // 更新忙线程数量
        DEBUG_INFO("[INFO] Thread [%ld] start working...\n", pthread_self());
        pthread_mutex_lock(&pool->mtx_busy);
        pool->busy_num++;
        pthread_mutex_unlock(&pool->mtx_busy);

        task.function(task.arg);
        free(task.arg);
        task.arg = NULL;

        DEBUG_INFO("[INFO] Thread [%ld] end working...\n", pthread_self());
        pthread_mutex_lock(&pool->mtx_busy);
        pool->busy_num--;
        pthread_mutex_unlock(&pool->mtx_busy);
    }
}

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

    ret = pthread_create(&pool->manager_id, NULL, manager, pool);
    if (ret != 0)
    {
        DEBUG_INFO("[ERROR] pthread_create()\n");
        goto err;
    }
    for (i = 0; i < pool->min_num; i++)
    {
        ret = pthread_create(&pool->worker_ids[i], NULL, worker, pool);
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

int thread_pool_destory(thread_pool_t *pool)
{
    if (pool == NULL)
    {
        return -1;
    }

    int i = 0;

    // 关闭线程池
    pool->shutdown = 1;
    // 阻塞回收管理者线程
    pthread_join(pool->manager_id, NULL);
    // 唤醒阻塞的消费者线程
    for (i = 0; i < pool->live_num; i++)
    {
        pthread_cond_signal(&pool->cond_empty);
    }

    // 释放堆内存
    if (pool->task_queue)
    {
        free(pool->task_queue);
        pool->task_queue = NULL;
    }
    if (pool->worker_ids)
    {
        free(pool->worker_ids);
        pool->worker_ids = NULL;
    }

    pthread_mutex_destroy(&pool->mtx_busy);
    pthread_mutex_destroy(&pool->mtx_pool);
    pthread_cond_destroy(&pool->cond_empty);
    pthread_cond_destroy(&pool->cond_full);
    free(pool);
    pool = NULL;
    return 0;
}

void thread_pool_add_task(thread_pool_t *pool, void (*func)(void *), void *arg)
{
    pthread_mutex_lock(&pool->mtx_pool);

    while (pool->q_size == pool->q_capacity && !pool->shutdown)
    {
        pthread_cond_wait(&pool->cond_full, &pool->mtx_pool);
    }

    if (pool->shutdown)
    {
        pthread_mutex_unlock(&pool->mtx_pool);
        return;
    }

    pool->task_queue[pool->q_rear].function = func;
    pool->task_queue[pool->q_rear].arg = arg;
    pool->q_rear = (pool->q_rear + 1) % pool->q_capacity;
    pool->q_size++;

    pthread_cond_signal(&pool->cond_empty);
    pthread_mutex_unlock(&pool->mtx_pool);
}

int thread_pool_busy_num(thread_pool_t *pool)
{
    pthread_mutex_lock(&pool->mtx_busy);
    int busy_num = pool->busy_num;
    pthread_mutex_unlock(&pool->mtx_busy);
    return busy_num;
}

int thread_pool_alive_num(thread_pool_t *pool)
{
    pthread_mutex_lock(&pool->mtx_pool);
    int live_num = pool->live_num;
    pthread_mutex_unlock(&pool->mtx_pool);
    return live_num;
}