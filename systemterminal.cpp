#include "systemterminal.hpp"

#include <unistd.h>


int SystemTerminal::read(int file_descriptor,char *buffer,size_t size)
{
  return ::read(file_descriptor,buffer,size);
}


int SystemTerminal::write(int file_descriptor,const char *buffer,int size)
{
  return ::write(file_descriptor,buffer,size);
}
