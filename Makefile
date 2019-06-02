CXXFLAGS=-W -Wall -Wundef -pedantic -std=c++17 -D_GLIBCXX_DEBUG=1 -g -MD -MP

all: run_unit_tests terminal_manualtest messaging_manualtest

run_unit_tests: \
  fakesockets_test.pass \
  messagetesting_test.pass \
  messageservice_test.pass

%.pass: %
	./$*
	touch $@

MESSAGETESTING=messagetesting.o messageservice.o terminal.o
FAKESOCKETS=fakesockets.o internetaddress.o fakefiledescriptorallocator.o
SYSTEMSOCKETS=systemsockets.o internetaddress.o

fakesockets_test: fakesockets_test.o $(FAKESOCKETS)
	$(CXX) $(LDFLAGS) -o $@ $^

messagetesting_test: messagetesting_test.o faketerminal.o \
  $(FAKESOCKETS) $(MESSAGETESTING)
	$(CXX) $(LDFLAGS) -o $@ $^

messageservice_test: messageservice_test.o messageservice.o $(FAKESOCKETS)
	$(CXX) $(LDFLAGS) -o $@ $^

terminal_manualtest: terminal_manualtest.o terminal.o systemterminal.o
	$(CXX) $(LDFLAGS) -o $@ $^

messaging_manualtest: messaging_manualtest.o systemterminal.o \
  $(SYSTEMSOCKETS) $(MESSAGETESTING)
	$(CXX) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o *.d *.pass *_test *_manualtest

-include *.d
