//
// Created by AyanMR on 26-3-11.
//

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <condition_variable>
#include <mutex>
#include <future>
#include <vector>
#include <atomic>
#include <queue>
#include <functional>
#include <stdexcept>
#include <ranges>

class ThreadPool
{
    public:
        ThreadPool(size_t thread_num);

        template < typename F , typename... Args >
        auto enqueue(F &&f, Args &&... args) -> std::future < std::invoke_result_t < F , Args... > >;


        ~ThreadPool();

        std::atomic < bool > isStop;

    private:
        std::vector < std::thread >             threads;
        std::mutex                              task_mutex;
        std::condition_variable                 cv;
        std::queue < std::function < void() > > tasks;
};

inline ThreadPool::ThreadPool(const size_t thread_num)
    : isStop(false)
{
    for (int i = 0 ; i < thread_num ; ++i)
    {
        threads.emplace_back([=] {
            while (true)
            {
                std::function < void() > task; {
                    std::unique_lock lock(task_mutex);
                    cv.wait(lock, [=] { return isStop || !tasks.empty(); });
                    if (isStop && tasks.empty())
                        break;
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }
}

template < typename F , typename... Args >
auto ThreadPool::enqueue(F &&f, Args &&... args) -> std::future < std::invoke_result_t < F , Args... > >
{
    using result_type = std::invoke_result_t < F , Args... >;
    auto task = std::make_shared < std::packaged_task < result_type() > >(std::bind(std::forward < F >(f), std::forward < Args >(args)...));
    std::future < result_type > result = task->get_future(); {
        std::unique_lock lock(task_mutex);
        if (isStop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
        tasks.emplace([task] { (*task)(); });
    }
    cv.notify_one();
    return result;
}


inline ThreadPool::~ThreadPool()
{ {
        std::unique_lock lock(task_mutex);
        isStop = true;
    }
    cv.notify_all();
    std::ranges::for_each(threads.begin(), threads.end(), [] (auto &t) { t.join(); });
}


#endif //THREADPOOL_H
