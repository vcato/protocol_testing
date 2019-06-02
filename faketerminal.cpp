#include "faketerminal.hpp"

#include <optional>


using std::optional;
using std::string;


static optional<size_t> maybeNewlinePosition(const Buffer &input_buffer)
{
  size_t n = input_buffer.size();

  for (size_t i=0; i!=n; ++i) {
    if (input_buffer.peek(i) == '\n') {
      return i;
    }
  }

  return std::nullopt;
}


int FakeTerminal::read(int file_descriptor,char *buffer,size_t size)
{
  assert(file_descriptor == input_file_descriptor);
  assert(!tty.input_buffer.isEmpty());
  optional<size_t> maybe_newline_position =
    maybeNewlinePosition(tty.input_buffer);

  if (!maybe_newline_position) {
    // No newline -- why are we reading?
    assert(false);
  }

  size_t newline_position = *maybe_newline_position;

  if (newline_position < size) {
    tty.input_buffer.get(buffer,newline_position + 1);
    return newline_position + 1;
  }

  // Read up to the first newline.
  assert(false);
}


int
  FakeTerminal::write(
    int file_descriptor,const char * buffer,int size
  )
{
  assert(file_descriptor == error_file_descriptor);
  output_member += string(buffer,size);
  return size;
}


bool FakeTerminal::haveNewlineInBuffer() const
{
  return !!maybeNewlinePosition(tty.input_buffer);
}


bool FakeTerminal::canRead() const
{
  return haveNewlineInBuffer();
}


void FakeTerminal::select(FakeSelectParams &params)
{
  if (params.readIsSet(input_file_descriptor)) {
    if (!canRead()) {
      params.clearRead(input_file_descriptor);
    }
  }
}


int FakeTerminal::nFileDescriptors() const
{
  return input_file_descriptor + 1;
}


void FakeTerminal::addInput(const std::string &s)
{
  for (char c : s) {
    tty.input_buffer.put(&c,1);
  }
}


const string& FakeTerminal::output() const
{
  return output_member;
}


void FakeTerminal::clearOutput()
{
  output_member = "";
}
