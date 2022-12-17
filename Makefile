TARGET?=vcd
all: $(TARGET)
install: $(TARGET); install -v -D $< $(DESTDIR)/usr/bin/$<
clean: ; $(RM) $(TARGET)
