// ---------------------------------------------------------------------
// pion:  a Boost C++ framework for building lightweight HTTP interfaces
// ---------------------------------------------------------------------
// Copyright (C) 2007-2014 Splunk Inc.  (https://github.com/splunk/pion)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#include <chrono>
#include <cstdint>

#include "pion/scheduler.hpp"

namespace pion {


// members of scheduler
    
const uint32_t   scheduler::DEFAULT_NUM_THREADS = 8;
const uint32_t   scheduler::NSEC_IN_SECOND = 1000000000; // (10^9)
const uint32_t   scheduler::MICROSEC_IN_SECOND = 1000000;    // (10^6)
const uint32_t   scheduler::KEEP_RUNNING_TIMER_SECONDS = 5;

scheduler::scheduler() :
m_logger(PION_GET_LOGGER("pion.scheduler")),
m_num_threads(DEFAULT_NUM_THREADS),
m_active_users(0),
m_is_running(false) { }

scheduler::~scheduler() { }

void scheduler::startup() { }

void scheduler::shutdown(void) {
    // lock mutex for thread safety
    std::unique_lock<std::mutex> scheduler_lock(m_mutex);
    
    if (m_is_running) {
        
        PION_LOG_INFO(m_logger, "Shutting down the thread scheduler");
        
        while (m_active_users > 0) {
            // first, wait for any active users to exit
            PION_LOG_INFO(m_logger, "Waiting for " << m_active_users << " scheduler users to finish");
            m_no_more_active_users.wait(scheduler_lock);
        }

        // shut everything down
        m_is_running = false;
        stop_services();
        stop_threads();
        finish_services();
        finish_threads();
        
        PION_LOG_INFO(m_logger, "The thread scheduler has shutdown");

        // Make sure anyone waiting on shutdown gets notified
        m_scheduler_has_stopped.notify_all();
        
    } else {
        
        // stop and finish everything to be certain that no events are pending
        stop_services();
        stop_threads();
        finish_services();
        finish_threads();
        
        // Make sure anyone waiting on shutdown gets notified
        // even if the scheduler did not startup successfully
        m_scheduler_has_stopped.notify_all();
    }
}

void scheduler::join(void) {
    std::unique_lock<std::mutex> scheduler_lock(m_mutex);
    while (m_is_running) {
        // sleep until scheduler_has_stopped condition is signaled
        m_scheduler_has_stopped.wait(scheduler_lock);
    }
}

bool scheduler::is_running() const {
    return m_is_running;
}

void scheduler::set_num_threads(const uint32_t n) {
    m_num_threads = n;
}

uint32_t scheduler::get_num_threads() const {
    return m_num_threads;
}

void scheduler::set_logger(logger log_ptr) {
    m_logger = log_ptr;
}

logger scheduler::get_logger(void) {
    return m_logger;
}

void scheduler::post(std::function<void()> work_func) {
    get_io_service().post(work_func);
}
    
void scheduler::keep_running(asio::io_service& my_service, asio::steady_timer& my_timer) {
    if (m_is_running) {
        // schedule this again to make sure the service doesn't complete
        my_timer.expires_from_now(std::chrono::seconds(KEEP_RUNNING_TIMER_SECONDS));
        my_timer.async_wait(std::bind(&scheduler::keep_running, this,
                                        std::ref(my_service), std::ref(my_timer)));
    }
}

void scheduler::add_active_user() {
    if (!m_is_running) startup();
    std::lock_guard<std::mutex> scheduler_lock(m_mutex);
    ++m_active_users;
}

void scheduler::remove_active_user() {
    std::lock_guard<std::mutex> scheduler_lock(m_mutex);
    if (--m_active_users == 0)
        m_no_more_active_users.notify_all();
}

void scheduler::sleep(uint32_t sleep_sec, uint32_t sleep_nsec) {
    auto nanos = std::chrono::nanoseconds{sleep_sec * 1000000000 + sleep_nsec};
    std::this_thread::sleep_for(nanos);
}

void scheduler::process_service_work(asio::io_service& service) {
    while (m_is_running) {
        try {
            service.run();
        } catch (std::exception& e) {
            (void) e;
            PION_LOG_ERROR(m_logger, e.what());
        } catch (...) {
            PION_LOG_ERROR(m_logger, "caught unrecognized exception");
        }
    }   
}

void scheduler::stop_services() { }

void scheduler::stop_threads() { }

void scheduler::finish_services() { }

void scheduler::finish_threads() { }


// multi_thread_scheduler definitions

multi_thread_scheduler::multi_thread_scheduler() { }

multi_thread_scheduler::~multi_thread_scheduler() { }

void multi_thread_scheduler::stop_threads() {
    if (!m_thread_pool.empty()) {
        PION_LOG_DEBUG(m_logger, "Waiting for threads to shutdown");

        // wait until all threads in the pool have stopped
        std::thread current_thread;
        for (std::vector<std::shared_ptr < std::thread>>::iterator i = m_thread_pool.begin();
                i != m_thread_pool.end(); ++i) {
            // make sure we do not call join() for the current thread,
            // since this may yield "undefined behavior"
            if ((*i)->get_id() != current_thread.get_id()) (*i)->join();
        }
    }
}

void multi_thread_scheduler::finish_threads() {
    m_thread_pool.clear();
}

// single_service_scheduler member functions

single_service_scheduler::single_service_scheduler() : 
m_service(), 
m_timer(m_service) { }

single_service_scheduler::~single_service_scheduler() {
    shutdown();
}

asio::io_service& single_service_scheduler::get_io_service() {
    return m_service;
}

void single_service_scheduler::startup() {
    // lock mutex for thread safety
    std::lock_guard<std::mutex> scheduler_lock(m_mutex);
    
    if (! m_is_running) {
        PION_LOG_INFO(m_logger, "Starting thread scheduler");
        m_is_running = true;
        
        // schedule a work item to make sure that the service doesn't complete
        m_service.reset();
        keep_running(m_service, m_timer);
        
        // start multiple threads to handle async tasks
        for (uint32_t n = 0; n < m_num_threads; ++n) {
            std::shared_ptr<std::thread> new_thread(new std::thread( std::bind(&scheduler::process_service_work,
                                                                                       this, std::ref(m_service)) ));
            m_thread_pool.push_back(new_thread);
        }
    }
}

void single_service_scheduler::stop_services() {
    m_service.stop();
}

void single_service_scheduler::finish_services() {
    m_service.reset();
}


// one_to_one_scheduler member functions

one_to_one_scheduler::one_to_one_scheduler() : 
m_service_pool(), 
m_next_service(0) { }

one_to_one_scheduler::~one_to_one_scheduler() {
    shutdown();
}

asio::io_service& one_to_one_scheduler::get_io_service() {
    std::lock_guard<std::mutex> scheduler_lock(m_mutex);
    while (m_service_pool.size() < m_num_threads) {
        std::shared_ptr<service_pair_type> service_ptr(new service_pair_type());
        m_service_pool.push_back(service_ptr);
    }
    if (++m_next_service >= m_num_threads)
        m_next_service = 0;
    assert(m_next_service < m_num_threads);
    return m_service_pool[m_next_service]->first;
}

asio::io_service& one_to_one_scheduler::get_io_service(uint32_t n) {
    assert(n < m_num_threads);
    assert(n < m_service_pool.size());
    return m_service_pool[n]->first;
}

void one_to_one_scheduler::startup() {
    // lock mutex for thread safety
    std::lock_guard<std::mutex> scheduler_lock(m_mutex);
    
    if (! m_is_running) {
        PION_LOG_INFO(m_logger, "Starting thread scheduler");
        m_is_running = true;
        
        // make sure there are enough services initialized
        while (m_service_pool.size() < m_num_threads) {
            std::shared_ptr<service_pair_type>  service_ptr(new service_pair_type());
            m_service_pool.push_back(service_ptr);
        }

        // schedule a work item for each service to make sure that it doesn't complete
        for (std::vector<std::shared_ptr<service_pair_type>>::iterator i = m_service_pool.begin(); i != m_service_pool.end(); ++i) {
            keep_running((*i)->first, (*i)->second);
        }
        
        // start multiple threads to handle async tasks
        for (uint32_t n = 0; n < m_num_threads; ++n) {
            std::shared_ptr<std::thread> new_thread(new std::thread( std::bind(&scheduler::process_service_work,
                                                                                       this, std::ref(m_service_pool[n]->first)) ));
            m_thread_pool.push_back(new_thread);
        }
    }
}

void one_to_one_scheduler::stop_services() {
    for (std::vector<std::shared_ptr<service_pair_type>>::iterator i = m_service_pool.begin(); i != m_service_pool.end(); ++i) {
        (*i)->first.stop();
    }
}

void one_to_one_scheduler::finish_services() {
    m_service_pool.clear();
}

one_to_one_scheduler::service_pair_type::service_pair_type() :
first(), 
second(first) { }

    
} // end namespace pion
