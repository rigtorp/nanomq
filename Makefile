
CXXFLAGS = -Wall -O3 -Iinclude

TARGETS = local_lat remote_lat local_thr remote_thr

all: $(TARGETS)

$(TARGETS): include/nmq.hpp

clean:
	rm -f *~ core
	rm -f $(TARGETS)