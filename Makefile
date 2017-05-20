CC = gcc

DEPDIR = .d
$(shell mkdir -p $(DEPDIR) > /dev/null)
POSTCOMPILE = mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

CFLAGS = $(DEPFLAGS) -ggdb3 -std=c99 -Wall -Wextra \
	 -Wpedantic -D_POSIX_C_SOURCE=200112L \
	 -iquote include -imacros config.h -include bool.h

EXEs = battle_client battle_server
COMMONOBJs = console.o sighandler.o netutil.o match.o game_client.o
COBJs = $(COMMONOBJs) proto.o battle_client.o
SOBJs = $(COMMONOBJs) server_proto.o list.o hashtable.o client_list.o battle_server.o
OBJs = $(COBJs) $(SOBJs)


.PHONY: all clean
.PRECIOUS: $(DEPDIR)/%.d


%.o: %.c
%.o: %.c $(DEPDIR)/%.d
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)



all: $(EXEs)

battle_client: $(COBJs)

battle_server: $(SOBJs)

clean:
	-rm -f $(DEPDIR)/*.d $(OBJs) $(EXEs)


server_proto.o: CFLAGS += -DBATTLE_SERVER
server_proto.o: proto.c $(DEPDIR)/%.d
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)


$(DEPDIR)/%.d: ;

-include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS))))
