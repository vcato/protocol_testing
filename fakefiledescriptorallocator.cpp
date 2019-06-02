#include "fakefiledescriptorallocator.hpp"

#include <cassert>
#include <algorithm>


int FakeFileDescriptorAllocator::allocate()
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


void FakeFileDescriptorAllocator::deallocate(int fd)
{
  assert(allocated_set[fd]);
  allocated_set[fd] = false;
}
