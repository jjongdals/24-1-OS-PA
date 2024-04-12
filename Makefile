TARGET	= mash
CFLAGS	= -g -c -D_POSIX_C_SOURCE -D_GNU_SOURCE -D_XOPEN_SOURCE=700
CFLAGS += -std=c99 -Wall -Wextra -Wno-unused-parameter -Werror
LDFLAGS	=

<<<<<<< HEAD
all: mash toy
=======
all: mash toy pipe
>>>>>>> upstream/main

mash: pa1.o mash.o parser.o
	gcc $(LDFLAGS) $^ -o $@

toy: toy.o
	gcc $(LDFLAGS) $^ -o $@

<<<<<<< HEAD
=======
pipe: pipe.o
	gcc $(LDFLAGS) $^ -o $@

>>>>>>> upstream/main
%.o: %.c
	gcc $(CFLAGS) $< -o $@

.PHONY: clean
clean:
<<<<<<< HEAD
	rm -rf $(TARGET) toy *.o *.dSYM
=======
	rm -rf $(TARGET) toy pipe *.o *.dSYM
>>>>>>> upstream/main


.PHONY: test-run
test-run: $(TARGET) toy testcases/test-run
<<<<<<< HEAD
	./$< < testcases/test-run
=======
	./$< -q < testcases/test-run
>>>>>>> upstream/main

.PHONY: test-cd
test-cd: $(TARGET) testcases/test-cd
	./$< -q < testcases/test-cd

.PHONY: test-alias
test-alias: $(TARGET) testcases/test-alias
	./$< -q < testcases/test-alias

.PHONY: test-pipe
<<<<<<< HEAD
test-pipe: $(TARGET) testcases/test-pipe
=======
test-pipe: $(TARGET) pipe testcases/test-pipe
>>>>>>> upstream/main
	./$< -q < testcases/test-pipe

.PHONY: test-combined
test-combined: $(TARGET) testcases/test-combined
	./$< -q < testcases/test-combined

.PHONY: test-all
test-all: test-run test-cd test-alias test-pipe test-combined
