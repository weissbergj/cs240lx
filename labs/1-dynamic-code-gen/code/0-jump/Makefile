NAME := header

# check that your path is defined.
ifndef CS240LX_2022_PATH
$(error CS240LX_2022_PATH is not set: this should contain the absolute path to where this directory is.  Define it in your shell's initialiation.  For example, .tcshrc for tcsh or .bashrc for bash)
endif

include $(CS240LX_2022_PATH)/libpi/defs.mk

CFLAGS += -Wno-unused-function -Wno-unused-variable 

# if you want to compile everything in a directory, do something like this.
#   SOURCES := $(wildcard ./*.c)
#   OBJS := $(patsubst %.c, %.o, $(SOURCES))
OBJS := $(NAME).o

# add all the .h's as dependencies, libpi and the makefile.
DEPS := $(MEMMAP) $(LPI) $(wildcard ../unix-side/*.h) ./memmap ./memmap.header

# uncomment the different tests.
all: libpi $(OBJS) $(NAME).bin run

libpi: 
	@ # @make -s -C $(LPP)
	make -C $(LPP)

$(OBJS): $(DEPS)
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.S
	$(CC) -c $(CPP_ASFLAGS) $< -o $@

# rules to generate .elf->.list->.bin
%.elf: $(MEMMAP) $(OBJS) $(DEPS) 
	$(LD) $(START) $(OBJS) -T ./memmap -o $*.elf  $(LPI)
%.list: %.elf
	$(OD) -D   $*.elf > $*.list
%.bin: %.list
	$(OCP) $*.elf -O binary $*.bin

run:
	my-install ./$(NAME).bin

tags: 
	ctags *.[chSs] $(LPP)/*.[ch] $(LPP)/*.[chSs]

clean:
	rm -rf $(BUILD_DIR) $(NAME) *~ tags *.o *.bin *.elf *.list *.img Makefile.bak  *.d

.PHONY: libpi all tags clean run

# stop make from deleting these suffix'd files.
.PRECIOUS: %.list %.elf
