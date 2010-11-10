
CXXFLAGS = -Wall -O3 -Iinclude

TARGETS = local_lat remote_lat local_thr remote_thr

all: $(TARGETS)

clean:
	rm -f *~
	rm -f $(TARGETS)