#include "tcp-state-machine.h"

std::vector<std::vector<
    std::tuple<Event, State, Stage,
               // < 0 discard, > 0 no discard, == 0 transstate
               std::function<int(TcpStateMachine &, const TcpHeader *,
                                 uint16_t)>, // Check
               std::function<void(TcpStateMachine &, const TcpHeader *,
                                 uint16_t)>, // Update
               std::function<void(TcpStateMachine &)>, // action on success
               std::function<void(TcpStateMachine &)> // action on failure
              > > > TcpStateMachine::tranform_rule_;