#ifndef SYSTEMSELECTOR_HPP_
#define SYSTEMSELECTOR_HPP_


#include "selector.hpp"


struct SystemSelectParams {
  int n_fds;
  fd_set read_fds;
  fd_set write_fds;
  fd_set except_fds;
  timeval timeout;

  SystemSelectParams()
  : n_fds(0),
    timeout{0,0}
  {
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);
  }

  void setupSelect()
  {
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);
    n_fds = 0;
    timeout.tv_sec = std::numeric_limits<time_t>::max();
    timeout.tv_usec = 999999;
  }

  void doSelect()
  {
    n_fds = select(n_fds, &read_fds, &write_fds, &except_fds, &timeout);
  }

  void setFD(int fd,fd_set &set)
  {
    FD_SET(fd,&set);

    if (fd >= n_fds) {
      n_fds = fd + 1;
    }
  }

  void setRead(int fd) { setFD(fd,read_fds); }
  void setWrite(int fd) { setFD(fd,write_fds); }
  bool readIsSet(int fd) const { return FD_ISSET(fd,&read_fds); }
  bool writeIsSet(int fd) const { return FD_ISSET(fd,&write_fds); }
};


class SystemSelector : public AbstractSelector {
  private:
    SystemSelectParams select_params;
    BasicSelectParamsWrapper<SystemSelectParams>
      select_params_wrapper{select_params};

    SelectParamsInterface &_selectParams() override
    {
      return select_params_wrapper;
    }

    void _setupSelect() override
    {
      select_params.setupSelect();
    }

    void _doSelect() override
    {
      select_params.doSelect();
    }
};


#endif /* SYSTEMSELECTOR_HPP_ */
