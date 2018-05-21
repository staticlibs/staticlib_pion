/*
 * Copyright 2015, alex at staticlibs.net
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// ---------------------------------------------------------------------
// pion:  a Boost C++ framework for building lightweight HTTP interfaces
// ---------------------------------------------------------------------
// Copyright (C) 2007-2014 Splunk Inc.  (https://github.com/splunk/pion)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#ifndef STATICLIB_PION_SCHEDULER_HPP
#define STATICLIB_PION_SCHEDULER_HPP

#include <cassert>
#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "asio.hpp"

#include "staticlib/config.hpp"

#include "staticlib/pion/logger.hpp"

namespace staticlib { 
namespace pion {

/**
 * Scheduler, combines Boost.ASIO with a managed thread pool for scheduling server threads
 */
class scheduler {
    /**
     * Mutex to make class thread-safe
     */
    std::mutex mutex;

    /**
     * Primary logging interface used by this class
     */
    logger log;

    /**
     * Condition triggered when there are no more active users
     */
    std::condition_variable no_more_active_users;

    /**
     * Condition triggered when the scheduler has stopped
     */
    std::condition_variable scheduler_has_stopped;

    /**
     * Total number of worker threads in the pool
     */
    uint32_t num_threads;

    /**
     * The scheduler will not shutdown until there are no more active users
     */
    uint32_t active_users;

    /**
     * True if the thread scheduler is running
     */
    bool running;

    /**
     * Pool of threads used to perform work
     */
    std::vector<std::unique_ptr<std::thread>> thread_pool;

    /**
     * Service used to manage async I/O events
     */
    asio::io_service asio_service;

    /**
     * Timer used to periodically check for shutdown
     */
    asio::steady_timer timer;
    
    /**
     * Hook function, that is called after each scheduled thread will exit
     */
    std::function<void() /* noexcept */> thread_stop_hook;
    
public:

    /**
     * Constructor
     */
    scheduler(uint32_t number_of_threads);
    
    /**
     * Deleted copy constructor
     */
    scheduler(const scheduler&) = delete;

    /**
     * Deleted copy assignment operator
     */
    scheduler& operator=(const scheduler&) = delete;
    
    /**
     * Destructor
     */
    ~scheduler();

    /**
     * Starts the thread scheduler (this is called automatically when necessary)
     */
    void startup();
    
    /**
     * Stops the thread scheduler (this is called automatically when the program exits)
     */
    void shutdown();

    /**
     * The calling thread will sleep until the scheduler has stopped
     */
    void join();
    
    /**
     * Registers an active user with the thread scheduler.  Shutdown of the
     * scheduler is deferred until there are no more active users.  This
     * ensures that any work queued will not reference destructed objects
     */
    void add_active_user();

    /**
     * Unregisters an active user with the thread scheduler
     */
    void remove_active_user();
    
    /**
     * Returns true if the scheduler is running
     * 
     * @return whether scheduler is running
     */
    bool is_running() const;
    
    /**
     * Returns an async I/O service used to schedule work
     * 
     * @return asio service
     */
    asio::io_service& get_io_service();
    
    /**
     * Schedules work to be performed by one of the pooled threads
     *
     * @param work_func work function to be executed
     */
    void post(std::function<void()> work_func);
    
    /**
     * Thread function used to keep the io_service running
     *
     * @param my_service IO service used to re-schedule keep_running()
     * @param my_timer deadline timer used to keep the IO service active while running
     */
    void keep_running(asio::io_service& my_service, asio::steady_timer& my_timer);
    
    /**
     * Puts the current thread to sleep for a specific period of time
     *
     * @param sleep_sec number of entire seconds to sleep for
     * @param sleep_nsec number of nanoseconds to sleep for (10^-9 in 1 second)
     */
    static void sleep(uint32_t sleep_sec, uint32_t sleep_nsec);

    /**
     * puts the current thread to sleep for a specific period of time, or until
     * a wakeup condition is signaled
     *
     * @param wakeup_condition if signaled, the condition will wakeup the thread early
     * @param wakeup_lock scoped lock protecting the wakeup condition
     * @param sleep_sec number of entire seconds to sleep for
     * @param sleep_nsec number of nanoseconds to sleep for (10^-9 in 1 second)
     */
    template <typename ConditionType, typename LockType>
    inline static void sleep(ConditionType& wakeup_condition, LockType& wakeup_lock,
                             uint32_t sleep_sec, uint32_t sleep_nsec) {
        auto nanos = std::chrono::nanoseconds{sleep_sec * 1000000000 + sleep_nsec};
        wakeup_condition.wait_for(wakeup_lock, nanos);
    }
    
    /**
     * processes work passed to the asio service & handles uncaught exceptions
     * 
     * @param service asio service
     */
    void process_service_work(asio::io_service& service);

    /**
     * Setter for hook function, that is called from each worker thread
     * just before that worker thread is going to exit
     * 
     * @param hook hook function
     */
    void set_thread_stop_hook(std::function<void() /* noexcept */> hook);
    
private:
    /**
     * Stops all services used to schedule work
     */
    void stop_services();
    
    /**
     * Stops all threads used to perform work
     */
    void stop_threads();

    /**
     * Finishes all services used to schedule work
     */
    void finish_services();

    /**
     * Finishes all threads used to perform work
     */
    void finish_threads();
    
};
        
} // namespace
}

#endif // STATICLIB_PION_SCHEDULER_HPP
