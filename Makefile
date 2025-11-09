TARGET?=vcd
all: $(TARGET)
$(TARGET):
	zig build-exe vcd.zig -O ReleaseFast -femit-bin=$(TARGET)
install: $(TARGET); install -v -D $< $(DESTDIR)/usr/bin/$<
clean: ; $(RM) $(TARGET) && $(RM) -rf zig-cache zig-out
