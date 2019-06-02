#ifndef SYSTEMTERMINAL_HPP_
#define SYSTEMTERMINAL_HPP_

#include "terminal.hpp"


struct SystemTerminal : Terminal {
  int inputFileDescriptor() const override { return 0; }
  int errorFileDescriptor() const override { return 2; }

  int read(int file_descriptor,char *buffer,size_t) override;
  int write(int file_descriptor,const char *buffer,int size) override;
};


#endif /* SYSTEMTERMINAL_HPP_ */
