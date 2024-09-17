#include "thread_pool.h"

void do_task(void *arg)
{
    int num = *(int *)arg;
    printf("Thread %ld is working. num = %d\n", pthread_self(), num);
    sleep(1);
}

int main()
{
    int i = 0;
    // 创建线程池
    thread_pool_t *pool = NULL;

    pool = thread_pool_create(3, 10, 100);
    for (i = 0; i < 100; i++)
    {
        int *num = (int *)malloc(sizeof(int));
        *num = i + 100;
        thread_pool_add_task(pool, do_task, num);
    }

    sleep(30);

    thread_pool_destory(pool);
    return 0;
}