#include "terminal.hpp"
#include "buffer.hpp"
#include "fakefiledescriptorallocator.hpp"
#include "fakeselectable.hpp"


struct FakeTty {
  Buffer input_buffer;

  FakeTty() : input_buffer(/*capacity*/1024) {}
};


struct FakeTerminal : Terminal, FakeSelectable {
  const int input_file_descriptor;
  const int error_file_descriptor;
  FakeTty tty;
  std::string output_member;

  FakeTerminal(FakeFileDescriptorAllocator &file_descriptor_allocator)
  : input_file_descriptor(file_descriptor_allocator.allocate()),
    error_file_descriptor(file_descriptor_allocator.allocate())
  {
  }

  int inputFileDescriptor() const override
  {
    return input_file_descriptor;
  }

  int errorFileDescriptor() const override
  {
    return error_file_descriptor;
  }

  void addInput(const std::string &);
  const std::string &output() const;
  void clearOutput();

  int read(int file_descriptor,char *buffer,size_t size) override;
  int write(int file_descriptor,const char *buffer,int size) override;

  void select(FakeSelectParams &) override;
  int nFileDescriptors() const override;
  bool canRead() const;
  bool haveNewlineInBuffer() const;
};

