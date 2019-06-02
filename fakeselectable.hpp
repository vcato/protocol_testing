#ifndef FAKESELECTABLE_HPP_
#define FAKESELECTABLE_HPP_

#include <float.h>


struct FakeSelectParams {
  std::vector<bool> read_set;
  std::vector<bool> write_set;
  std::vector<bool> except_set;
  float timeout = 0;

  void setupSelect(int n_fds)
  {
    read_set.assign(n_fds,false);
    write_set.assign(n_fds,false);
    except_set.assign(n_fds,false);
    timeout = FLT_MAX;
  }

  void setRead(int fd) { read_set[fd] = true; }
  void setWrite(int fd) { write_set[fd] = true; }
  void clearRead(int fd) { read_set[fd] = false; }
  bool readIsSet(int fd) const { return read_set[fd]; }
  bool writeIsSet(int fd) const { return write_set[fd]; }
};


struct FakeSelectable {
  virtual void select(FakeSelectParams &) = 0;
  virtual int nFileDescriptors() const = 0;
};


#endif /* FAKESELECTABLE_HPP_ */
