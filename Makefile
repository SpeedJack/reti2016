# This file is part of reti2016.
#
# reti2016 is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# reti2016 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# See file LICENSE for more details.

CC = gcc

DEPDIR = .d
$(shell mkdir -p $(DEPDIR) > /dev/null)
POSTCOMPILE = mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

STDFLAGS = $(DEPFLAGS) -std=c99 -D_POSIX_C_SOURCE=200112L \
	   -iquote include -imacros config.h -include bool.h
CFLAGS = $(STDFLAGS) -ggdb3 -Wall -Wextra -Wpedantic

COMPILE.c = $(CC) $(CFLAGS) $(TARGET_ARCH) -c

EXEs = battle_client battle_server
COMMONOBJs = console.o sighandler.o netutil.o game_client.o
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

nodebug: CFLAGS = $(STDFLAGS) -O1 -DNDEBUG
nodebug: $(EXEs)

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
