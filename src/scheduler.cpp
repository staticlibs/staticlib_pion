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

#include "staticlib/pion/scheduler.hpp"

#include <cstdint>
#include <chrono>

namespace staticlib { 
namespace pion {

// members of scheduler
    
scheduler::scheduler(uint32_t number_of_threads) :
log(STATICLIB_PION_GET_LOGGER("staticlib.pion.scheduler")),
num_threads(number_of_threads),
active_users(0),
running(false),
asio_service(),
timer(asio_service),
thread_stop_hook([]() STATICLIB_NOEXCEPT {}) { }

scheduler::~scheduler() {
    shutdown();
}

void scheduler::startup() {
    // lock mutex for thread safety
    std::lock_guard<std::mutex> scheduler_lock(mutex);

    if (!running) {
        STATICLIB_PION_LOG_INFO(log, "Starting thread scheduler");
        running = true;

        // schedule a work item to make sure that the service doesn't complete
        asio_service.reset();
        keep_running(asio_service, timer);

        // start multiple threads to handle async tasks
        for (uint32_t n = 0; n < num_threads; ++n) {
            std::unique_ptr<std::thread> new_thread(new std::thread([this]() {
                this->process_service_work(this->asio_service);
                this->thread_stop_hook();
            }));
            thread_pool.emplace_back(std::move(new_thread));
        }
    }
}

void scheduler::shutdown(void) {
    // lock mutex for thread safety
    std::unique_lock<std::mutex> scheduler_lock{mutex};
    
    if (running) {
        
        STATICLIB_PION_LOG_INFO(log, "Shutting down the thread scheduler");
        
        while (active_users > 0) {
            // first, wait for any active users to exit
            STATICLIB_PION_LOG_INFO(log, "Waiting for " << active_users << " scheduler users to finish");
            no_more_active_users.wait(scheduler_lock);
        }

        // shut everything down
        running = false;
        stop_services();
        stop_threads();
        finish_services();
        finish_threads();
        
        STATICLIB_PION_LOG_INFO(log, "The thread scheduler has shutdown");

        // Make sure anyone waiting on shutdown gets notified
        scheduler_has_stopped.notify_all();
        
    } else {
        
        // stop and finish everything to be certain that no events are pending
        stop_services();
        stop_threads();
        finish_services();
        finish_threads();
        
        // Make sure anyone waiting on shutdown gets notified
        // even if the scheduler did not startup successfully
        scheduler_has_stopped.notify_all();
    }
}

void scheduler::join(void) {
    std::unique_lock<std::mutex> scheduler_lock(mutex);
    while (running) {
        // sleep until scheduler_has_stopped condition is signaled
        scheduler_has_stopped.wait(scheduler_lock);
    }
}

bool scheduler::is_running() const {
    return running;
}

void scheduler::post(std::function<void()> work_func) {
    get_io_service().post(work_func);
}
    
void scheduler::keep_running(asio::io_service& my_service, asio::steady_timer& my_timer) {
    if (running) {
        // schedule this again to make sure the service doesn't complete
        my_timer.expires_from_now(std::chrono::seconds(5));
        auto cb = [this, &my_service, &my_timer](const std::error_code&){
            this->keep_running(my_service, my_timer);
        };
        my_timer.async_wait(std::move(cb));
    }
}

void scheduler::add_active_user() {
    if (!running) startup();
    std::lock_guard<std::mutex> scheduler_lock(mutex);
    ++active_users;
}

void scheduler::remove_active_user() {
    std::lock_guard<std::mutex> scheduler_lock(mutex);
    if (--active_users == 0) {
        no_more_active_users.notify_all();
    }
}

void scheduler::sleep(uint32_t sleep_sec, uint32_t sleep_nsec) {
    auto nanos = std::chrono::nanoseconds{sleep_sec * 1000000000 + sleep_nsec};
    std::this_thread::sleep_for(nanos);
}

void scheduler::process_service_work(asio::io_service& service) {
    while (running) {
        try {
            service.run();
        } catch (std::exception& e) {
            (void) e;
            STATICLIB_PION_LOG_ERROR(log, e.what());
        } catch (...) {
            STATICLIB_PION_LOG_ERROR(log, "caught unrecognized exception");
        }
    }   
}

void scheduler::set_thread_stop_hook(std::function<void() /* noexcept */> hook) {
    std::unique_lock<std::mutex> scheduler_lock{mutex};
    this->thread_stop_hook = hook;
}

void scheduler::stop_services() {
    asio_service.stop();
}

void scheduler::stop_threads() {
    if (!thread_pool.empty()) {
        STATICLIB_PION_LOG_DEBUG(log, "Waiting for threads to shutdown");

        // wait until all threads in the pool have stopped
        auto current_id = std::this_thread::get_id();
        for (auto& th_ptr : thread_pool) {
            // make sure we do not call join() for the current thread,
            // since this may yield "undefined behavior"
            std::thread::id tid = th_ptr->get_id();
            if (tid != current_id) {
                th_ptr->join();
            }
        }
    }
}

void scheduler::finish_services() {
    asio_service.reset();
}

void scheduler::finish_threads() {
    thread_pool.clear();
}

asio::io_service& scheduler::get_io_service() {
    return asio_service;
}

} // namespace
}
