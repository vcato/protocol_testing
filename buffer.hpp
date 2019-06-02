#ifndef BUFFER_HPP_
#define BUFFER_HPP_


#include <cassert>
#include <vector>


struct Buffer {
  Buffer(size_t capacity)
  : bytes(capacity)
  {
  }

  bool isEmpty() const
  {
    return size_member == 0;
  }

  size_t size() const { return size_member; }

  char peek(size_t i) const
  {
    assert(i < size_member);
    return bytes[(read_position + i) % bytes.size()];
  }

  bool isFull() const
  {
    assert(size_member <= bytes.size());
    return (size_member == bytes.size());
  }

  int put(const void *buf,int len)
  {
    assert(size_member < bytes.size());
    int n_bytes_put = 0;
    const char *char_buf = static_cast<const char *>(buf);

    while (size_member < bytes.size() && n_bytes_put < len) {
      int write_position = (read_position + size_member) % bytes.size();
      bytes[write_position] = char_buf[n_bytes_put];
      ++size_member;
      ++n_bytes_put;
    }

    assert(n_bytes_put > 0);
    return n_bytes_put;
  }

  int get(void *buf,int len)
  {
    int n_bytes_got = 0;
    char *char_buf = static_cast<char *>(buf);

    while (size_member > 0 && n_bytes_got < len) {
      char_buf[n_bytes_got] = bytes[read_position];
      read_position = (read_position + 1) % bytes.size();
      --size_member;
      ++n_bytes_got;
    }

    return n_bytes_got;
  }

private:
  std::vector<char> bytes;
  int read_position = 0;
  size_t size_member = 0;
};


#endif /* BUFFER_HPP_ */
