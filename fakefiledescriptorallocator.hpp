#ifndef FAKEFILEDESCRIPTORALLOCATOR_HPP_
#define FAKEFILEDESCRIPTORALLOCATOR_HPP_

#include <algorithm>


class FakeFileDescriptorAllocator {
  public:
    int allocate()
    {
      auto iter = std::find(allocated_set.begin(),allocated_set.end(),false);

      if (iter != allocated_set.end()) {
        *iter = true;
        return iter - allocated_set.begin();
      }

      int new_fd = allocated_set.size();
      allocated_set.push_back(true);
      return new_fd;
    }

    void deallocate(int fd)
    {
      assert(allocated_set[fd]);
      allocated_set[fd] = false;
    }

  private:
    std::vector<bool> allocated_set;
};


#endif /* FAKEFILEDESCRIPTORALLOCATOR_HPP_ */
