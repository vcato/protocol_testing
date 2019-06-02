CXXFLAGS=-W -Wall -Wundef -pedantic -std=c++17 -D_GLIBCXX_DEBUG=1 -g -MD -MP

all: run_unit_tests terminal_manualtest messaging_manualtest

run_unit_tests: \
  fakesockets_test.pass \
  messagetesting_test.pass \
  messageservice_test.pass

%.pass: %
	./$*
	touch $@

fakesockets_test: fakesockets_test.o fakesockets.o internetaddress.o
	$(CXX) $(LDFLAGS) -o $@ $^

messagetesting_test: messagetesting_test.o messagetesting.o \
  fakesockets.o messageservice.o internetaddress.o faketerminal.o terminal.o
	$(CXX) $(LDFLAGS) -o $@ $^

messageservice_test: messageservice_test.o fakesockets.o messageservice.o \
  internetaddress.o
	$(CXX) $(LDFLAGS) -o $@ $^

terminal_manualtest: terminal_manualtest.o systemterminal.o terminal.o
	$(CXX) $(LDFLAGS) -o $@ $^

messaging_manualtest: messaging_manualtest.o \
  internetaddress.o systemsockets.o fakesockets.o messageservice.o \
  systemterminal.o messagetesting.o terminal.o
	$(CXX) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o *.d *.pass *_test *_manualtest

-include *.d
