#ifndef FAKEFILEDESCRIPTORALLOCATOR_HPP_
#define FAKEFILEDESCRIPTORALLOCATOR_HPP_

#include <vector>


class FakeFileDescriptorAllocator {
  public:
    int allocate();
    void deallocate(int fd);

  private:
    std::vector<bool> allocated_set;
};


#endif /* FAKEFILEDESCRIPTORALLOCATOR_HPP_ */
