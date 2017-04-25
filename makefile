boost_dir = -I /usr/include/boost/include
boost_lib_dir = -L /usr/include/boost/lib

boost_library = -lboost_system -lboost_thread -lboost_context\
	-lboost_coroutine -lboost_serialization

library = -pthread

dir = $(boost_dir) $(boost_lib_dir)

source = tcp.cc tcp-buffer.cc tcp-state-machine.cc network.cc

main:
	g++ $(dir) main.cc $(source) $(library) -Wall #-Wconversion