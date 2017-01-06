CC = gcc
DEPDIR = .d
$(shell mkdir -p $(DEPDIR) > /dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
CFLAGS = $(DEPFLAGS) -ggdb3 -std=c99 -Wall -Wextra \
	 -Wpedantic -D_POSIX_C_SOURCE=200112L
POSTCOMPILE = mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d
EXEs = battle_client battle_server
COBJs = battle_client.o
SOBJs = battle_server.o
OBJs = $(COBJs) $(SOBJs)

.PHONY: all clean
.PRECIOUS: $(DEPDIR)/%.d

%.o: %.c
%.o: %.c $(DEPDIR)/%.d
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

all: $(EXEs)

-include $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS)))

battle_client: $(CBOJs)

battle_server: $(SOBJs)

clean:
	-rm -f .d/*.d $(OBJs) $(EXEs)

$(DEPDIR)/%.d: ;
