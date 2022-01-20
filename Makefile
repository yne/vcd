#
# Makefile for vcd
#

TARGET=vcd

SRCS=vcd.c
DEPS=$(SRCS:.c=.dep)
OBJS=$(SRCS:.c=.o)

#
#
#

.PHONY: default
default: build

.PHONY: all
all: all

.PHONY: build
build: $(TARGET)

$(TARGET): $(OBJS)

$(OBJS): $(SRCS)

.PHONY: install
install: build
	install -v -D vcd $(DESTDIR)/usr/bin/vcd

.PHONY: clean
clean:
	$(RM) $(TARGET)
	$(RM) $(OBJS)
	$(RM) $(DEPS)


#
# EOF
#
