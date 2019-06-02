#include "terminal.hpp"


using std::string;


void Terminal::setupSelect(PreSelectParamsInterface &select_params)
{
  if (had_eof) {
    return;
  }

  select_params.setRead(inputFileDescriptor());

  if (!text_to_show.empty()) {
    select_params.setWrite(errorFileDescriptor());
  }
}


void
  Terminal::handleSelect(
    const PostSelectParamsInterface &select_params,
    EventInterface &event_handler
  )
{
  if (had_eof) {
    return;
  }

  int input_file_descriptor = inputFileDescriptor();
  int error_file_descriptor = errorFileDescriptor();

  if (select_params.readIsSet(input_file_descriptor)) {
    char buffer[256];
    int n_bytes_read = read(input_file_descriptor,buffer,sizeof buffer);

    if (n_bytes_read == 0) {
      event_handler.endOfFile();
      had_eof = true;
    }
    else {
      if (buffer[n_bytes_read - 1] == '\n') {
        event_handler.gotLine(
          line_received_so_far + Line(buffer,n_bytes_read - 1)
        );
        line_received_so_far = "";
      }
      else {
        line_received_so_far += Line(buffer,n_bytes_read);
      }
    }
  }

  if (select_params.writeIsSet(error_file_descriptor)) {
    assert(!text_to_show.empty());

    int write_result =
      write(
        error_file_descriptor,
        text_to_show.c_str(),
        text_to_show.length()
      );

    if (write_result <= 0) {
      // Got an error or EOF.
      assert(false);
    }

    int n_bytes_written = write_result;

    text_to_show.erase(
      text_to_show.begin(), text_to_show.begin() + n_bytes_written
    );
  }
}


void Terminal::show(const string &s)
{
  text_to_show += s;
}
