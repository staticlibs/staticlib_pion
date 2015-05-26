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

#ifndef __PION_SCHEDULER_HEADER__
#define __PION_SCHEDULER_HEADER__

#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <cassert>
#include <cstdint>

#include "asio.hpp"

#include "pion/noncopyable.hpp"
#include "pion/config.hpp"
#include "pion/logger.hpp"

namespace pion {

/**
 * Scheduler, combines Boost.ASIO with a managed thread pool for scheduling server threads
 */
class PION_API scheduler : private pion::noncopyable {
    
protected:

    /**
     * Default number of worker threads in the thread pool
     */
    static const uint32_t DEFAULT_NUM_THREADS;

    /**
     * Number of nanoseconds in one full second (10 ^ 9)
     */
    static const uint32_t NSEC_IN_SECOND;

    /**
     * Number of microseconds in one full second (10 ^ 6)
     */
    static const uint32_t MICROSEC_IN_SECOND;

    /**
     * Number of seconds a timer should wait for to keep the IO services running
     */
    static const uint32_t KEEP_RUNNING_TIMER_SECONDS;

    /**
     * Mutex to make class thread-safe
     */
    std::mutex m_mutex;

    /**
     * Primary logging interface used by this class
     */
    logger m_logger;

    /**
     * Condition triggered when there are no more active users
     */
    std::condition_variable m_no_more_active_users;

    /**
     * Condition triggered when the scheduler has stopped
     */
    std::condition_variable m_scheduler_has_stopped;

    /**
     * Total number of worker threads in the pool
     */
    uint32_t m_num_threads;

    /**
     * The scheduler will not shutdown until there are no more active users
     */
    uint32_t m_active_users;

    /**
     * True if the thread scheduler is running
     */
    bool m_is_running;    
    
public:

    /**
     * Constructor
     */
    scheduler();
    
    /**
     * Virtual destructor
     */
    virtual ~scheduler();

    /**
     * Starts the thread scheduler (this is called automatically when necessary)
     */
    virtual void startup();
    
    /**
     * Stops the thread scheduler (this is called automatically when the program exits)
     */
    virtual void shutdown();

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
     * Sets the number of threads to be used (these are shared by all servers)
     * 
     * @param n number of threads
     */
    void set_num_threads(const uint32_t n);
    
    /**
     * Returns the number of threads currently in use
     * 
     * @return number of threads
     */
    uint32_t get_num_threads() const;

    /**
     * Sets the logger to be used
     * 
     * @param log_ptr logger instance
     */
    void set_logger(logger log_ptr);

    /**
     * Returns the logger currently in use
     * 
     * @return logger instance
     */
    logger get_logger(void);
    
    /**
     * Returns an async I/O service used to schedule work
     * 
     * @return asio service
     */
    virtual asio::io_service& get_io_service(void) = 0;
    
    /**
     * Schedules work to be performed by one of the pooled threads
     *
     * @param work_func work function to be executed
     */
    virtual void post(std::function<void()> work_func);
    
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

protected:

    /**
     * Stops all services used to schedule work
     */
    virtual void stop_services();
    
    /**
     * Stops all threads used to perform work
     */
    virtual void stop_threads();

    /**
     * Finishes all services used to schedule work
     */
    virtual void finish_services();

    /**
     * Finishes all threads used to perform work
     */
    virtual void finish_threads();
    
};

    
/**
 * Uses a pool of threads to perform work
 */
class PION_API multi_thread_scheduler : public scheduler {
    
protected:

    /**
     * Pool of threads used to perform work
     */
    std::vector<std::shared_ptr<std::thread>> m_thread_pool;
    
public:
    
    /**
     * Constructor
     */
    multi_thread_scheduler();
    
    /**
     * Virtual destructor
     */
    virtual ~multi_thread_scheduler();

    
protected:
    
    /**
     * Stops all threads used to perform work
     */
    virtual void stop_threads();

    /**
     * Finishes all threads used to perform work
     */
    virtual void finish_threads();
};
    
    
/**
 * Uses a single IO service to schedule work
 */
class PION_API single_service_scheduler : public multi_thread_scheduler {
    
protected:

    /**
     * Service used to manage async I/O events
     */
    asio::io_service m_service;

    /**
     * Timer used to periodically check for shutdown
     */
    asio::steady_timer m_timer;    
    
public:
    
    /**
     * Constructor
     */
    single_service_scheduler();
    
    /// virtual destructor
    /**
     * Virtual destructor, calls shutdown
     */
    virtual ~single_service_scheduler();
    
    /**
     * Returns an async I/O service used to schedule work
     * 
     * @return asio service
     */
    virtual asio::io_service& get_io_service(void);
    
    /**
     * Starts the thread scheduler (this is called automatically when necessary)
     */
    virtual void startup();
    
protected:
    
    /**
     * Stops all services used to schedule work
     */
    virtual void stop_services();
    
    /**
     * Finishes all services used to schedule work
     */
    virtual void finish_services();
};
    

/**
 * Uses a single IO service for each thread
 */
class PION_API one_to_one_scheduler : public multi_thread_scheduler {

protected:

    /**
     * Struct for a pair object where first is an IO service and second is a deadline timer
     */
    struct service_pair_type {
        service_pair_type(void) : first(), second(first) { }
        asio::io_service first;
        asio::steady_timer second;
    };
    
    /**
     * Pool of IO services used to schedule work
     */
    std::vector<std::shared_ptr<service_pair_type>> m_service_pool;

    /**
     * The next service to use for scheduling work
     */
    uint32_t m_next_service;
    
public:
    
    /**
     * Constructor
     */
    one_to_one_scheduler();
    
    /**
     * Virtual destructor
     */
    virtual ~one_to_one_scheduler();
    
    /**
     * Returns an async I/O service used to schedule work
     * 
     * @return asio service
     */
    virtual asio::io_service& get_io_service();
    
    /**
     * returns an async I/O service used to schedule work (provides direct
     * access to avoid locking when possible)
     *
     * @param n integer number representing the service object
     */
    virtual asio::io_service& get_io_service(uint32_t n);

    /**
     * Starts the thread scheduler (this is called automatically when necessary)
     */
    virtual void startup();    
    
protected:
    
    /**
     * Stops all services used to schedule work
     */
    virtual void stop_services();
        
    /**
     * Finishes all services used to schedule work
     */
    virtual void finish_services();
};
    
} // end namespace pion

#endif
