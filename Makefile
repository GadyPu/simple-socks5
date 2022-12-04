.PHONY: clean
CC = g++
CFLAGS = -O2
LDFLAG =
RM = rm
EXE = tinysocks5
OBJS = logger.o tinysocks5.o
$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAG)
%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^
clean:
	$(RM) $(EXE) $(OBJS)
