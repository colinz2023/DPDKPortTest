#ifndef SRB_H_
#define SRB_H_

#define CompilerBarrier()            \
  do {                               \
    asm volatile("" : : : "memory"); \
  } while (0)

inline void GetPowerOf2(uint32_t len, uint32_t* len2) {
  *len2 = 1;
  while (*len2 < len) {
    *len2 *= 2;
  }
}

template <typename T>
class Sfrb {
 public:
  Sfrb(uint32_t size) {
    GetPowerOf2(size, &size_);
    curr_ = 0;
    data_ = new T[size_];
  }
  ~Sfrb() { delete data_; }

  uint32_t Set(T* data) {
    uint32_t curr = ++curr_;
    uint32_t index = curr & (size_ - 1);
    CompilerBarrier();
    memcpy(&data_[index], data, sizeof(T));
    return curr;
  }

  int GetCurr(T* data) {
    uint32_t curr = curr_ - 1;
    uint32_t index = curr & (size_ - 1);
    CompilerBarrier();
    memcpy(data, &data_[index], sizeof(T));
    return 0;
  }

  int GetOffset(T* data, uint32_t offset) {
    uint32_t curr = curr_;
    uint32_t index = (curr - offset) & (size_ - 1);
    CompilerBarrier();
    if (offset >= size_) {
      return -1;
    }
    if (curr < size_ && curr < offset) {
      return -1;
    }
    memcpy(data, &data_[index], sizeof(T));
    return 0;
  }

 private:
  volatile uint32_t curr_;
  uint32_t size_;
  T* data_;
};

#endif
