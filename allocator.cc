template <size_t block_size = 4096*16>
class Buffer {
 public:
  class SegmentManager;
  class Segment {
   public:
    friend class SegmentManager;
    
    struct SegmentHeader {
      char occupied;
      char head;
      char tail;
      
      char padding[5];
      uint8_t *memory_end;
    };
    
    Segment() = delete;
    Segment(uint8_t *memory_begin, uint8_t *memory_end,
            char occupied, char head, char tail) {
      assert(reinterpret_cast<std::uintptr_t>(memory_begin)%8 == 0);
      assert(memory_end-memory_begin > sizeof(SegmentHeader));
      
      header_ = reinterpret_cast<SegmentHeader *>(*memory_begin);
      header_->occupied = occupied;
      header_->head = head;
      header_->tail = tail;
      header_->memory_end = memory_end;
    }
    
    Segment(const Segment &) = delete;
    Segment(Segment &&) = default;
    
    Segment &operator=(const Segment &) = delete;
    Segment &operator=(Segment &&)  = default;
    
    ~Segment() = default;
    
    static auto Begin(uint8_t *p) {
      return p+sizeof(SegmentHeader);
    }
    
    auto Begin() {
      return Begin(reinterpret_cast<uint8_t *>(header_));
    }
    auto Begin() const {
      return Begin(reinterpret_cast<const uint8_t *>(header_));
    }

    auto End() const {
      return header_->memory_end;
    }
    
    auto Memory() {
      return reinterpret_cast<uint8_t *>(header_);
    }
    
    size_t Size() const {
      assert(End() >= Begin());
      return End() - Begin();
    }
    
    bool IsHead() {
      return header_->head;
    }
    bool IsTail() {
      return header_->tail;
    }
    
   private:
    Segment(uint8_t *p) : header_(reinterpret_cast<uint8_t *>(p)) {}
    SegmentHeader *header_ = nullptr;
  };
  
  class SegmentManager {
   public:
    SegmentManager() = default;
    SegmentManager(const SegmentManager &) = delete;
    SegmentManager(SegmentManager &&) = default;
    ~SegmentManager() = default;
    
    SegmentManager &operator=(const SegmentManager &) = delete;
    SegmentManager &operator=(SegmentManager &&) = default;
    
    Segment *AddSegment(uint8_t memory_begin, uint8_t memory_end,
                        bool head, bool tail) {
      auto result = segments_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(Buffer::Begin(memory_begin)),
        std::forward_as_tuple(memory_begin, memory_end, 0, head, tail));
      if (result.second == false)
        throw std::runtime_error("Buffer::AddSegment critical memory error");
      
      segments_index_[size_t].insert(&*result.first);
      return &*result.first->first;
    }
    
    void AddSegment(Segment *segment) {
      
    }
    
    Segment *MatchAndRemoveSegment(uint8_t *memory_begin, uint8_t *memory_end) {
      return nullptr
    }
    
   private:
    std::pair<Segment *, Segment *> SplitSegment(
        Segment &segment, size_t size) {
      if (segment.Size() > size+64) {
        const auto memory1 = segment.Memory();
        const auto end1 = segment.Begin() + size;
        const auto head1 = segment.head();
        const auto memory2 = end1;
        const auto end2 = segment.End();
        const auto tail2 = segment.tail();
        
        segments_.erase(segment.Begin());
        auto result = segments_index_.find(segment.Size());
        if (result == segments_index_.end())
          throw std::runtime_error("Buffer::SplitSegment critical memory error");
        result->second->erase(segment.Begin());
        if (result->second->size() == 0)
          segments_index_.erase(result);
        
        return {
            AddSegment(memory1, memory2, head1, false),
            AddSegment(memory1, memory2, false, tail2)};
      } else {
        return {&segment, nullptr};
      }
    }
  
    std::map<uint8_t *, Segment> segments_;
    std::map<size_t, std::set<uint8_t *> > segments_index_;
  };
  
  Buffer() = default;
  Buffer(const Buffer &) = delete;
  Buffer(Buffer &&) = default;
  
  Buffer &operator=(const Buffer &) = delete;
  Buffer &operator=(Buffer &&) = default;
  
  ~Buffer() = default;
  
  uint8_t *Allocate(int size) {
    assert(size > 0);

    return nullptr;
  }
  
 private:
  void AddBlock() {
    auto *memory = new uint8_t[block_size];
    if (raw_buffers_.insert(memory).second == false)
      throw std::runtime_error("Buffer::AddBlock critical memory error");
    
    segments_.AddSegment(memory, memory+block_size, true, true);
  }

  std::set<uint8_t *> raw_buffers_;
  SegmentManager segments_;
};