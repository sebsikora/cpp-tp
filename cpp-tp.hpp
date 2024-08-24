// MIT No Attribution

// Copyright 2024 Dr Seb N.F. Sikora

// Permission is hereby granted, free of charge, to any person obtaining a copy of this
// software and associated documentation files (the "Software"), to deal in the Software
// without restriction, including without limitation the rights to use, copy, modify,
// merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


// cpp-tp.hpp - A thread pool class that manages worker threads that run
//              arbitrary callables


#include <functional>
#include <queue>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>


#ifdef VERBOSE
#include <iostream>
#endif


class ThreadPool {
public:

    // Constructor
    ThreadPool(bool auto_start, size_t worker_count = 0) :  // If worker_count == 0, = number of hardware threads
        m_stopped(true),
        m_waiting(false),
        m_pending_jobs(0)
    {
        if (auto_start) {
            start(worker_count);
        }
    }


    // Copy Constructor (deleted)
    ThreadPool(const ThreadPool&) = delete;


    // Destructor
    ~ThreadPool() {
        stop();
    }


    // Add a new job to the queue
    void addJob(std::function<void()> work_func) {
        std::lock_guard<std::mutex> m_lk(m_management_mutex);
        ++m_pending_jobs;
#ifdef VERBOSE
        std::string msg = "Adding job to queue - There are now " + std::to_string(m_pending_jobs) + " pending jobs.\n";
        std::cout << msg;
#endif
        m_job_queue.emplace(std::make_unique<std::function<void()>>(work_func));
        m_management_cv.notify_one();
    }
    

    // Start the threadpool
    // Set worker_count = 0 to use one thread per hardware supported thread
    bool start(size_t worker_count) {
        std::lock_guard<std::mutex> m_lk(m_management_mutex);

        if (!m_stopped) {
            return false;
        } else {
            m_stopped = false;
            
            // create worker threads
            if (worker_count == 0) {
                worker_count = std::thread::hardware_concurrency();
            }
            for (size_t id = 0; id < worker_count; ++id) {
                m_worker_threads.emplace_back(std::make_unique<std::thread>(&ThreadPool::workerLoop, this, id));
            }
            
            return true;
        }
        
    }
    

    // Stop the threadpool
    // Call to stop() will block until all running jobs have finished and been join()ed, set clear_queue = false
    // to leave pending jobs on the queue or clear_queue = true to delete pending jobs.
    bool stop(bool clear_queue = true) {
        {
            std::lock_guard<std::mutex> m_lk(m_management_mutex);

            if (m_stopped) {
                return false;
            } else {
                m_stopped = true;
                if (clear_queue) {
                    m_job_queue = {};
                }
            }
            m_management_cv.notify_all();   // wake any sleeping worker threads
        }
        // join all delete all worker threads
        for (const auto& thread_ptr : m_worker_threads) {
            thread_ptr->join();
        }
        m_worker_threads.clear();
        
        return true;
    }
    

    // Check if worker threads are running
    bool isStopped() {
        return m_stopped;
    }


    // Get the count of queued and running jobs
    size_t pendingJobs() {
        std::unique_lock<std::mutex> m_lk(m_management_mutex);
        return m_pending_jobs;
    }


    // Get the count of queued jobs
    size_t queuedJobs() {
        std::unique_lock<std::mutex> m_lk(m_management_mutex);
        return m_job_queue.size();
    }


    // Get the count of running jobs
    size_t runningJobs() {
        std::unique_lock<std::mutex> m_lk(m_management_mutex);
        return m_pending_jobs - m_job_queue.size();
    }
    

    // Wait for all pending jobs to complete
    void wait() {
        std::unique_lock<std::mutex> m_lk(m_management_mutex);

        if (m_pending_jobs > 0) {
            m_waiting = true;

            m_counter_cv.wait(m_lk, [this](){ return (m_pending_jobs == 0); });
            m_waiting = false;
        }
    }


    // Clear the job queue and return the number of cleared jobs
    size_t clearQueue() {
        std::lock_guard<std::mutex> m_lk(m_management_mutex);

        size_t queued_jobs_cleared = m_job_queue.size();
        m_pending_jobs -= queued_jobs_cleared;
        m_job_queue = {};

        return queued_jobs_cleared;
    }

private:

    // Worker thread runtime loop
    void workerLoop(size_t id) {
#ifdef VERBOSE
        std::string msg = "Worker " + std::to_string(id) + " started.\n";
        std::cout << msg;
#endif
        std::unique_ptr<std::function<void()>> job;

        while(true) {
            {
                std::unique_lock<std::mutex> m_lk(m_management_mutex);
                m_management_cv.wait(m_lk, [this](){ return ((!m_job_queue.empty()) || (m_stopped)); });
                
                if (!m_stopped) {
                    job.reset(m_job_queue.front().release());   // fetch a job from the queue
                    m_job_queue.pop();                          // immediately delete remaining invalid ptr
#ifdef VERBOSE
                    msg = "Worker " + std::to_string(id) + " starting job.\n";
                    std::cout << msg;
#endif
                } else {
                    break;
                }
            }

            (*job)();   // run the job and return the result via callback

            {
                std::lock_guard<std::mutex> m_lk(m_management_mutex);
                --m_pending_jobs;
#ifdef VERBOSE
                msg = "Worker " + std::to_string(id) + " finished - There are now " + std::to_string(m_pending_jobs) + " pending jobs.\n";
                std::cout << msg;
#endif
                if ((m_pending_jobs == 0) && (m_waiting)) { // if we are wait()ing for all jobs to complete and there
                    m_counter_cv.notify_one();              // are no pending jobs then notify the wait()ing thread.
                }
            }
        }
#ifdef VERBOSE
        msg = "Worker " + std::to_string(id) + " stopped.\n";
        std::cout << msg;
#endif
    }
    

    // Class data
    bool m_stopped;
    bool m_waiting;
    
    std::queue<std::unique_ptr<std::function<void()>>> m_job_queue;
    std::vector<std::unique_ptr<std::thread>> m_worker_threads;

    std::mutex m_management_mutex;
    std::condition_variable m_management_cv;

    ssize_t m_pending_jobs;
    std::condition_variable m_counter_cv;
    
};
