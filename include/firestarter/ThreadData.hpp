#ifndef INCLUDE_FIRESTARTER_THREADDATA_HPP
#define INCLUDE_FIRESTARTER_THREADDATA_HPP

#define THREAD_WAIT 1
#define THREAD_WORK 2
#define THREAD_INIT 3
#define THREAD_STOP 4
#define THREAD_INIT_FAILURE 0xffffffff

/* DO NOT CHANGE! the asm load-loop tests if load-variable is == 0 */
#define LOAD_LOW 0
/* DO NOT CHANGE! the asm load-loop continues until the load-variable is != 1 */
#define LOAD_HIGH 1
#define LOAD_STOP 2

#include <firestarter/Environment/Environment.hpp>

#include <mutex>

namespace firestarter {

class ThreadData {
public:
  ThreadData(int id, environment::Environment *environment,
             volatile unsigned long long *loadVar, unsigned long long period)
      : addrHigh(loadVar), period(period), _id(id), _environment(environment),
        _config(
            new environment::platform::Config(*environment->selectedConfig)){};
  ~ThreadData(){};

  const int &id = _id;
  environment::Environment *const &environment = _environment;
  environment::platform::Config *const &config = _config;

  int comm = THREAD_WAIT;
  bool ack = false;
  std::mutex mutex;
  unsigned long long *addrMem;
  volatile unsigned long long *addrHigh;
  unsigned long long buffersizeMem;
  unsigned long long iterations = 0;
  unsigned long long flops;
  unsigned long long start_tsc;
  unsigned long long stop_tsc;
  // period in usecs
  // used in low load routine to sleep 1/100th of this time
  unsigned long long period;

private:
  int _id;
  environment::Environment *_environment;
  environment::platform::Config *_config;
};

} // namespace firestarter

#endif
