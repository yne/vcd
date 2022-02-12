TARGET?=vcd
SRCS:=$(wildcard *.c)
all: $(TARGET)
$(TARGET): $(SRCS); $(CC) $(CFLAGS) $(SRCS) -o $@
install: $(TARGET); install -v -D $< $(DESTDIR)/usr/bin/$<
clean: ; $(RM) $(TARGET)
