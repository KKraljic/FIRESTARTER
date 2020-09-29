/******************************************************************************
 * FIRESTARTER - A Processor Stress Test Utility
 * Copyright (C) 2020 TU Dresden, Center for Information Services and High
 * Performance Computing
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/\>.
 *
 * Contact: daniel.hackenberg@tu-dresden.de
 *****************************************************************************/

#include <firestarter/Firestarter.hpp>

#include <cerrno>
#include <csignal>

#ifdef ENABLE_SCOREP
#include <SCOREP_User.h>
#endif

#define NSEC_PER_SEC 1000000000

#define DO_SLEEP(sleepret, target, remaining)                                  \
  do {                                                                         \
    if (watchdog_terminate) {                                                  \
      log::info() << "\nCaught shutdown signal, ending now ...";               \
      return EXIT_SUCCESS;                                                     \
    }                                                                          \
    sleepret = nanosleep(&target, &remaining);                                 \
    while (sleepret == -1 && errno == EINTR && !watchdog_terminate) {          \
      sleepret = nanosleep(&remaining, &remaining);                            \
    }                                                                          \
    if (sleepret == -1) {                                                      \
      switch (errno) {                                                         \
      case EFAULT:                                                             \
        log::error()                                                           \
            << "Found a bug in FIRESTARTER, error on copying for nanosleep";   \
        break;                                                                 \
      case EINVAL:                                                             \
        log::error()                                                           \
            << "Found a bug in FIRESTARTER, invalid setting for nanosleep";    \
        break;                                                                 \
      case EINTR:                                                              \
        log::info() << "\nCaught shutdown signal, ending now ...";             \
        break;                                                                 \
      default:                                                                 \
        log::error() << "Calling nanosleep: " << errno;                        \
        break;                                                                 \
      }                                                                        \
      set_load(LOAD_STOP);                                                     \
      if (errno == EINTR) {                                                    \
        return EXIT_SUCCESS;                                                   \
      } else {                                                                 \
        return EXIT_FAILURE;                                                   \
      }                                                                        \
    }                                                                          \
  } while (0)

using namespace firestarter;

namespace {
static volatile pthread_t watchdog_thread;
static volatile bool watchdog_terminate = false;
static volatile unsigned long long *loadvar;
} // namespace

void set_load(unsigned long long value) {
  // signal load change to workers
  *loadvar = value;
  __asm__ __volatile__("mfence;");
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#ifndef _WIN32
void sigalrm_handler(int signum) {}
#endif
#pragma GCC diagnostic pop
#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void sigterm_handler(int signum) {
  // required for cases load = {0,100}, which do no enter the loop
  set_load(LOAD_STOP);
  // exit loop
  // used in case of 0 < load < 100
  watchdog_terminate = true;

  pthread_kill(watchdog_thread, 0);
}
#pragma GCC diagnostic pop
#pragma clang diagnostic pop

int Firestarter::watchdogWorker(std::chrono::microseconds period,
                                std::chrono::microseconds load,
                                std::chrono::seconds timeout) {

  using clock = std::chrono::high_resolution_clock;
  using nsec = std::chrono::nanoseconds;
  using usec = std::chrono::microseconds;
  using sec = std::chrono::seconds;

  loadvar = &this->loadVar;

  watchdog_thread = pthread_self();

#ifndef _WIN32
  std::signal(SIGALRM, sigalrm_handler);
#endif

  std::signal(SIGTERM, sigterm_handler);
  std::signal(SIGINT, sigterm_handler);

  // values for wrapper to pthreads nanosleep
  int sleepret;
  struct timespec target, remaining;

  // calculate idle time to be the rest of the period
  auto idle = period - load;

  // elapsed time
  nsec time(0);

  // do no enter the loop if we do not have to set the load level periodically,
  // at 0 or 100 load.
  if (period > usec::zero()) {
    // this first time is critical as the period will be alligend from this
    // point
    std::chrono::time_point<clock> startTime = clock::now();

    // this loop will set the load level periodically.
    for (;;) {
      std::chrono::time_point<clock> currentTime = clock::now();

      // get the time already advanced in the current timeslice
      // this can happen if a load function does not terminates just on time
      nsec advance = std::chrono::duration_cast<nsec>(currentTime - startTime) %
                     std::chrono::duration_cast<nsec>(period);

      // subtract the advaned time from our timeslice by spilting it based on
      // the load level
      nsec load_reduction =
          (std::chrono::duration_cast<nsec>(load).count() * advance) /
          std::chrono::duration_cast<nsec>(period).count();
      nsec idle_reduction = advance - load_reduction;

      // signal high load level
      set_load(LOAD_HIGH);

      // calculate values for nanosleep
      nsec load_nsec = load - load_reduction;
      target.tv_nsec = load_nsec.count() % NSEC_PER_SEC;
      target.tv_sec = load_nsec.count() / NSEC_PER_SEC;

      // wait for time to be ellapsed with high load
#ifdef ENABLE_VTRACING
      VT_USER_START("WD_HIGH");
#endif
#ifdef ENABLE_SCOREP
      SCOREP_USER_REGION_BY_NAME_BEGIN("WD_HIGH",
                                       SCOREP_USER_REGION_TYPE_COMMON);
#endif
      DO_SLEEP(sleepret, target, remaining);
#ifdef ENABLE_VTRACING
      VT_USER_END("WD_HIGH");
#endif
#ifdef ENABLE_SCOREP
      SCOREP_USER_REGION_BY_NAME_END("WD_HIGH");
#endif

      // terminate if an interrupt by the user was fired
      if (watchdog_terminate) {
        set_load(LOAD_STOP);

        return EXIT_SUCCESS;
      }

      // signal low load
      set_load(LOAD_LOW);

      // calculate values for nanosleep
      nsec idle_nsec = idle - idle_reduction;
      target.tv_nsec = idle_nsec.count() % NSEC_PER_SEC;
      target.tv_sec = idle_nsec.count() / NSEC_PER_SEC;

      // wait for time to be ellapsed with low load
#ifdef ENABLE_VTRACING
      VT_USER_START("WD_LOW");
#endif
#ifdef ENABLE_SCOREP
      SCOREP_USER_REGION_BY_NAME_BEGIN("WD_LOW",
                                       SCOREP_USER_REGION_TYPE_COMMON);
#endif
      DO_SLEEP(sleepret, target, remaining);
#ifdef ENABLE_VTRACING
      VT_USER_END("WD_LOW");
#endif
#ifdef ENABLE_SCOREP
      SCOREP_USER_REGION_BY_NAME_END("WD_LOW");
#endif

      // increment elapsed time
      time += period;

      // exit when termination signal is received or timeout is reached
      if (watchdog_terminate || (timeout > sec::zero() && (time > timeout))) {
        set_load(LOAD_STOP);

        return EXIT_SUCCESS;
      }
    }
  }

  // if timeout is set, sleep for this time and stop execution.
  // else return and wait for sigterm handler to request threads to stop.
  if (timeout > sec::zero()) {
    target.tv_nsec = 0;
    target.tv_sec = timeout.count();

    DO_SLEEP(sleepret, target, remaining);

    set_load(LOAD_STOP);

    return EXIT_SUCCESS;
  }

  return EXIT_SUCCESS;
}
