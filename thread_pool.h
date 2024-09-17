#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#define OP_NUM 2

// 任务结构体
typedef struct task
{
    void (*function)(void *);
    void *arg;
} task_t;
// 线程池结构体
typedef struct thread_pool
{
    // 任务队列
    task_t *task_queue;
    int q_capacity; // 任务队列容量
    int q_size;     // 当前任务个数
    int q_front;    // 队列头
    int q_rear;     // 队列尾

    // 线程
    pthread_t manager_id;  // 管理者线程
    pthread_t *worker_ids; // 工作者线程
    int min_num;           // 最小线程数
    int max_num;           // 最大线程数
    int busy_num;          // 繁忙线程数
    int live_num;          // 存活的线程数
    int die_num;           // 需要销毁的线程数

    // 互斥锁和条件变量
    pthread_mutex_t mtx_pool;  // 锁整个线程池
    pthread_mutex_t mtx_busy;  // 锁busy_num
    pthread_cond_t cond_full;  // 任务队列是不是满了
    pthread_cond_t cond_empty; // 任务队列是不是空了

    // 销毁标志
    int shutdown; // 1销毁 0不销毁

} thread_pool_t;

void thread_exit(thread_pool_t *pool);
void *manager(void *arg);
void *worker(void *arg);
// 创建线程池并初始化
extern thread_pool_t *thread_pool_create(const int n_min, const int n_max, const int q_size);
// 销毁线程池
extern int thread_pool_destory(thread_pool_t *pool);
// 添加任务
extern void thread_pool_add_task(thread_pool_t *pool, void (*func)(void *), void *arg);
// 获取正在工作的线程个数
extern int thread_pool_busy_num(thread_pool_t *pool);
// 获取存活的线程个数
extern int thread_pool_alive_num(thread_pool_t *pool);

#endif