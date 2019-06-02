#ifndef TERMINAL_HPP_
#define TERMINAL_HPP_

#include <string>
#include "selector.hpp"


struct Terminal {
  bool had_eof = false;
  using Line = std::string;
  Line line_received_so_far;
  std::string text_to_show;

  struct EventInterface {
    virtual void gotLine(const Line &) = 0;
    virtual void endOfFile() = 0;
  };

  bool isActive() const { return !had_eof; }
  void setupSelect(PreSelectParamsInterface &select_params);
  void show(const std::string &);

  void
    handleSelect(
      const PostSelectParamsInterface &select_params,
      EventInterface &event_handler
    );

  virtual int inputFileDescriptor() const = 0;
  virtual int errorFileDescriptor() const = 0;
  virtual int read(int file_descriptor,char *buffer,size_t size) = 0;
  virtual int write(int file_descriptor,const char *buffer,int size) = 0;
};


#endif /* TERMINAL_HPP_ */
