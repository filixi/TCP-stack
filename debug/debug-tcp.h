#include "../tcp.h"

struct DebugTcp {
  void Run() {
    TestEstab();
  }

  void TestEstab() {
    TcpInternal internal1(1, nullptr, 20, 10);
    TcpInternal internal2(2, nullptr, 10, 20);
    
    internal1.state_.SetState(0, State::kEstab, Stage::kEstab);
    internal2.state_.SetState(0, State::kEstab, Stage::kEstab);
    
    internal1.peer_initial_seq_ = 100;
    internal1.peer_last_ack_ = 501;
    internal1.peer_window_ = 10240;
    
    internal1.host_initial_seq_ = 20;
    internal1.host_next_seq_ = 501;
    internal1.host_last_ack_ = 1001;
    internal1.host_window_ = 20480;
    
    
    internal2.peer_initial_seq_ = 20;
    internal2.peer_last_ack_ = 1001;
    internal2.peer_window_ = 20480;
    
    internal2.host_initial_seq_ = 100;
    internal2.host_next_seq_ = 1001;
    internal2.host_last_ack_ = 501;
    internal2.host_window_ = 10240;
    
    char buff[] = "abc";
    internal1.AddPackageForSending(TcpPackage(std::begin(buff), std::end(buff)));
    internal1.AddPackageForSending(TcpPackage(std::begin(buff), std::end(buff)));
    internal1.AddPackageForSending(TcpPackage(std::begin(buff), std::end(buff)));
    
    auto packages = internal1.GetPackagesForSending();
    std::vector<std::shared_ptr<NetworkPackage> >
        vtr_packages{packages.begin(), packages.end()};
    std::cerr << __func__ << ": packagesSize:" << packages.size() << std::endl;
    
    std::cerr << "-----------------------------------" << std::endl;
    
    std::cerr << internal2.ReceivePackage(TcpPackage(vtr_packages[2])) << std::endl;
    
    std::cerr << "-----------------------------------" << std::endl;
    
    std::cerr << internal2.ReceivePackage(TcpPackage(vtr_packages[1])) << std::endl;
    
    std::cerr << "-----------------------------------" << std::endl;
    
    std::cerr << internal2.ReceivePackage(TcpPackage(vtr_packages[0])) << std::endl;
    
    std::cerr << "-----------------------------------" << std::endl;
    
    packages = internal2.GetPackagesForSending();
    assert(!packages.empty());
    auto package = packages.front();
    packages.pop_front();
    std::cerr << internal1.ReceivePackage(TcpPackage(package)) << std::endl;
  }
  
  void TestFin() {
    TcpInternal internal1(1, nullptr, 20, 10);
    TcpInternal internal2(2, nullptr, 10, 20);
    
    internal1.state_.SetState(0, State::kEstab, Stage::kEstab);
    internal2.state_.SetState(0, State::kEstab, Stage::kEstab);
    
    internal1.peer_initial_seq_ = 100;
    internal1.peer_last_ack_ = 501;
    internal1.peer_window_ = 10240;
    
    internal1.host_initial_seq_ = 20;
    internal1.host_next_seq_ = 501;
    internal1.host_last_ack_ = 1001;
    internal1.host_window_ = 20480;
    
    
    internal2.peer_initial_seq_ = 20;
    internal2.peer_last_ack_ = 1001;
    internal2.peer_window_ = 20480;
    
    internal2.host_initial_seq_ = 100;
    internal2.host_next_seq_ = 1001;
    internal2.host_last_ack_ = 501;
    internal2.host_window_ = 10240;
    
    char buff[] = "abc";
    internal1.SendFin();
    
    auto packages = internal1.GetPackagesForSending();
    std::vector<std::shared_ptr<NetworkPackage> >
        vtr_packages{packages.begin(), packages.end()};
        
    std::cerr << internal2.ReceivePackage(TcpPackage(vtr_packages[0])) << std::endl;
    
    std::cerr << internal2.state_ << std::endl;
  }
};