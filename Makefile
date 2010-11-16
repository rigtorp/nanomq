
CXXFLAGS = -Wall -O3 -Iinclude

TARGETS = local_lat remote_lat local_thr remote_thr tests

all: $(TARGETS)

$(TARGETS): include/nmq.hpp
tests: LDFLAGS += -lgtest -lpthread

test: tests
	@./tests

clean:
	rm -f *~ core
	rm -f $(TARGETS)