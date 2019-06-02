#include <iostream>
#include "terminal.hpp"
#include "processevents.hpp"
#include "systemterminal.hpp"
#include "systemselector.hpp"


using std::string;
using std::cerr;
using std::vector;


namespace {
class TerminalTest {
public:
  TerminalTest(Terminal &terminal_arg) : terminal(terminal_arg)
  {
  }

  EventSinkInterface &eventSink() { return event_sink; }

  bool isActive()
  {
    return terminal.isActive();
  }

private:
  struct TerminalEventHandler : SystemTerminal::EventInterface {
    TerminalTest &terminal_test;

    TerminalEventHandler(TerminalTest &terminal_test_arg)
    : terminal_test(terminal_test_arg)
    {
    }

    void gotLine(const string &line) override
    {
      terminal_test.gotLineFromTerminal(line);
    }

    void endOfFile() override
    {
    }
  };

  struct EventSink : EventSinkInterface {
    TerminalTest &terminal_test;

    EventSink(TerminalTest &terminal_test_arg)
    : terminal_test(terminal_test_arg)
    {
    }

    void setupSelect(PreSelectParamsInterface &pre_select) const
    {
      terminal_test.setupSelect(pre_select);
    }

    void handleSelect(const PostSelectParamsInterface &post_select)
    {
      terminal_test.handleSelect(post_select);
    }
  };

  Terminal &terminal;
  TerminalEventHandler event_handler{*this};
  EventSink event_sink{*this};

  void gotLineFromTerminal(const string &line)
  {
    cerr << "gotLine: " << line << "\n";
  }

  void setupSelect(PreSelectParamsInterface &pre_select)
  {
    terminal.setupSelect(pre_select);
  }

  void handleSelect(const PostSelectParamsInterface &post_select)
  {
    terminal.handleSelect(post_select,event_handler);
  }
};
}


static int runTerminalTest()
{
  SystemTerminal terminal;
  SystemSelector selector;
  TerminalTest terminal_test{terminal};
  vector<EventSinkInterface *> event_sinks = {&terminal_test.eventSink()};

  while (terminal_test.isActive()) {
    processEvents(selector,event_sinks);
  }

  return EXIT_SUCCESS;
}


int main()
{
  runTerminalTest();
}
