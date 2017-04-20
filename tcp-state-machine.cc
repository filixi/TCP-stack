#include "tcp-state-machine.h"

std::vector<std::vector<std::tuple<Event, State, Action, Stage> > >
      TcpStateMachine::tranform_rule_;