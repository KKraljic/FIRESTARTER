#include <firestarter/Firestarter.hpp>
#include <firestarter/Logging/Log.hpp>

#ifdef ENABLE_VTRACING
#include <vt_user.h>
#endif
#ifdef ENABLE_SCOREP
#include <SCOREP_User.h>
#endif

#include <cstdlib>
#include <functional>

using namespace firestarter;

int Firestarter::initThreads(bool lowLoad, unsigned long long period) {

  int returnCode;

  if (EXIT_SUCCESS != (returnCode = this->environment->setCpuAffinity(0))) {
    return EXIT_FAILURE;
  }

  // setup load variable to execute low or high load once the threads switch to
  // work.
  this->loadVar = lowLoad ? LOAD_LOW : LOAD_HIGH;

  // allocate buffer for threads
  auto threads = static_cast<pthread_t *>(std::aligned_alloc(
      64, this->environment->requestedNumThreads * sizeof(pthread_t)));

  for (unsigned long long i = 0; i < this->environment->requestedNumThreads; i++) {
    auto td = new ThreadData(i, this->environment, &this->loadVar, period);

    auto dataCacheSizeIt =
        td->config->platformConfig->dataCacheBufferSize.begin();
    auto ramBufferSize = td->config->platformConfig->ramBufferSize;

    td->buffersizeMem = (*dataCacheSizeIt + *std::next(dataCacheSizeIt, 1) +
                         *std::next(dataCacheSizeIt, 2) + ramBufferSize) /
                        td->config->thread / sizeof(unsigned long long);

    // create the thread
    if (EXIT_SUCCESS != (returnCode = pthread_create(
                             &threads[i], NULL, threadWorker, std::ref(td)))) {
      log::error() << "Error: pthread_create failed with returnCode "
                   << returnCode;
      return EXIT_FAILURE;
    }

    this->threads.push_back(std::make_pair(&threads[i], std::ref(td)));
  }

  this->signalThreads(THREAD_INIT);

  return EXIT_SUCCESS;
}

void Firestarter::signalThreads(int comm) {
  bool ack;

  // start the work
  for (auto const &thread : this->threads) {
    auto td = thread.second;

    td->mutex.lock();
  }

  for (auto const &thread : this->threads) {
    auto td = thread.second;

    td->comm = comm;
    td->mutex.unlock();
  }

  for (auto const &thread : this->threads) {
    auto td = thread.second;

    do {
      td->mutex.lock();
      ack = td->ack;
      td->mutex.unlock();
    } while (!ack);

    td->mutex.lock();
    td->ack = false;
    td->mutex.unlock();
  }
}

void Firestarter::joinThreads(void) {
  // wait for threads after watchdog has requested termination
  for (auto const &thread : this->threads) {
    pthread_join(*thread.first, NULL);
  }
}

void Firestarter::printPerformanceReport(void) {
  // performance report
  unsigned long long startTimestamp = 0xffffffffffffffff;
  unsigned long long stopTimestamp = 0;

  unsigned long long iterations = 0;

  log::debug() << "\nperformance report:\n";

  for (auto const &thread : this->threads) {
    auto td = thread.second;

    log::debug() << "Thread " << td->id << ": " << td->iterations
                 << " iterations, tsc_delta: " << td->stop_tsc - td->start_tsc;

    if (startTimestamp > td->start_tsc) {
      startTimestamp = td->start_tsc;
    }
    if (stopTimestamp < td->stop_tsc) {
      stopTimestamp = td->stop_tsc;
    }

    iterations += td->iterations;
  }

  double runtime = (double)(stopTimestamp - startTimestamp) /
                   (double)this->environment->clockrate;
  double gFlops = (double)this->threads.front().second->config->payload->flops *
                  0.000000001 * (double)iterations / runtime;
  double bandwidth =
      (double)this->threads.front().second->config->payload->bytes *
      0.000000001 * (double)iterations / runtime;

  // format runtime, gflops and bandwidth %.2f
  const char *fmt = "%.2f";
  int size;

#define FORMAT(input)                                                          \
  size = std::snprintf(nullptr, 0, fmt, input);                                \
  std::vector<char> input##Vector(size + 1);                                   \
  std::snprintf(&input##Vector[0], input##Vector.size(), fmt, input);          \
  auto input##String = std::string(&input##Vector[0])

  FORMAT(runtime);
  FORMAT(gFlops);
  FORMAT(bandwidth);

#undef FORMAT

  log::debug()
      << "\n"
      << "total iterations: " << iterations << "\n"
      << "runtime: " << runtimeString << " seconds ("
      << stopTimestamp - startTimestamp << " cycles)\n"
      << "\n"
      << "estimated floating point performance: " << gFlopsString << " GFLOPS\n"
      << "estimated memory bandwidth*: " << bandwidthString << " GB/s\n"
      << "\n"
      << "* this estimate is highly unreliable if --function is used in order "
         "to "
         "select\n"
      << "  a function that is not optimized for your architecture, or if "
         "FIRESTARTER is\n"
      << "  executed on an unsupported architecture!";
}

void *Firestarter::threadWorker(void *threadData) {

  int old = THREAD_WAIT;

  auto td = (ThreadData *)threadData;

  for (;;) {
    td->mutex.lock();
    int comm = td->comm;
    td->mutex.unlock();

    if (comm != old) {
      old = comm;

      td->mutex.lock();
      td->ack = true;
      td->mutex.unlock();
    } else {
      continue;
    }

    switch (comm) {
    // allocate and initialize memory
    case THREAD_INIT:
      // set affinity
      td->environment->setCpuAffinity(td->id);

      // compile payload
      td->config->payload->compilePayload(
          td->config->payloadSettings,
          td->config->platformConfig->dataCacheBufferSize,
          td->config->platformConfig->ramBufferSize, td->config->thread, 1536);

      // allocate memory
      td->addrMem = static_cast<unsigned long long *>(std::aligned_alloc(
          64, td->buffersizeMem * sizeof(unsigned long long)));
      // TODO: handle error

      // call init function
      td->config->payload->init(td->addrMem, td->buffersizeMem);
      break;
    // perform stress test
    case THREAD_WORK:
      // record threads start timestamp
      td->start_tsc = td->environment->timestamp();

      // will be terminated by watchdog
      for (;;) {
        // call high load function
#ifdef ENABLE_VTRACING
        VT_USER_START("HIGH_LOAD_FUNC");
#endif
#ifdef ENABLE_SCOREP
        SCOREP_USER_REGION_BY_NAME_BEGIN("HIGH",
                                         SCOREP_USER_REGION_TYPE_COMMON);
#endif
        td->iterations = td->config->payload->highLoadFunction(
            td->addrMem, td->addrHigh, td->iterations);

        // call low load function
#ifdef ENABLE_VTRACING
        VT_USER_END("HIGH_LOAD_FUNC");
        VT_USER_START("LOW_LOAD_FUNC");
#endif
#ifdef ENABLE_SCOREP
        SCOREP_USER_REGION_BY_NAME_END("HIGH");
        SCOREP_USER_REGION_BY_NAME_BEGIN("LOW", SCOREP_USER_REGION_TYPE_COMMON);
#endif
        td->config->payload->lowLoadFunction(td->addrHigh, td->period);
#ifdef ENABLE_VTRACING
        VT_USER_END("LOW_LOAD_FUNC");
#endif
#ifdef ENABLE_SCOREP
        SCOREP_USER_REGION_BY_NAME_END("LOW");
#endif

        // terminate if master signals end of run and record stop timestamp
        if (*td->addrHigh == LOAD_STOP) {
          td->stop_tsc = td->environment->timestamp();

          pthread_exit(NULL);
        }
      }
      break;
    case THREAD_WAIT:
      break;
    case THREAD_STOP:
    default:
      pthread_exit(0);
    }
  }
}
