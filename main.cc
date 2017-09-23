
#include "state.h"
#include "timeout-queue.h"

int main() {
  TimeoutQueue queue;
  for (int i=0; i<8; ++i)
    queue.AsyncRun();

  std::function<void()> fn;
  fn = [&] {
        static int i = 0;
        std::clog << ++i << std::endl;
        if (i<5)
          queue.PushEvent(fn, std::chrono::seconds(1));
      };

  queue.PushEventWithFeedBack(fn, std::chrono::seconds(1));
  
  queue.WaitUntilAllDone();
  return 0;
}
