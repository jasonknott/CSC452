TARGET = libphase5.a
ASSIGNMENT = 452phase5
CC = gcc
AR = ar
COBJS = phase5.o p1.o libuser.o 
CSRCS = ${COBJS:.o=.c}
UNAME := $(shell uname -s)
PHASE1LIB = patrickphase1
PHASE2LIB = patrickphase2
PHASE3LIB = patrickphase3
PHASE4LIB = patrickphase4
#PHASE1LIB = patrickphase1debug
#PHASE2LIB = patrickphase2debug
#PHASE3LIB = patrickphase3debug
#PHASE4LIB = patrickphase4debug

HDRS = vm.h phase5.h

INCLUDE = ./usloss/include

CFLAGS = -Wall -g -std=gnu99 -I${INCLUDE} -I. #-Werror

ifeq ($(UNAME), Darwin)
        CFLAGS += -D_XOPEN_SOURCE
endif

LDFLAGS += -L. -L./usloss/lib

TESTDIR = testcases
# TESTS = test1 test2 test3 test4 simple1 simple2 simple3 simple4 simple5 \
#         chaos replace1 outOfSwap replace2 gen clock quit 
TESTS = simple1 simple2 simple3 simple4 simple5 \
        disk0 disk1 test5 
# LIBS = $(TESTDIR)/Tconsole.o -lpatrickphase4 -lpatrickphase3 -lpatrickphase2 \
#        -lpatrickphase1 -lusloss -lpatrickphase1 -lpatrickphase2 \
#        -lpatrickphase3 -lpatrickphase4 -lphase5 -luser
LIBS = $(TESTDIR)/Tconsole.o -lpatrickphase4 -lpatrickphase3 -lpatrickphase2 \
       -lpatrickphase1 -lusloss -lpatrickphase1 -lpatrickphase2 \
       -lpatrickphase3 -lpatrickphase4 -lphase5

$(TARGET):	$(COBJS)
		$(AR) -r $@ $(COBJS) 

Tconsole: $(TESTDIR)/Tconsole.c
	$(CC) $(CFLAGS) -c $(TESTDIR)/Tconsole.c -o $(TESTDIR)/Tconsole.o

#$(TESTS):	$(TARGET) $(TESTDIR)/$$@.c
$(TESTS):	$(TARGET) Tconsole
	$(CC) $(CFLAGS) -c $(TESTDIR)/$@.c
	$(CC) $(LDFLAGS) -o $@ $@.o $(LIBS)

clean:
	rm -f $(COBJS) $(TARGET) test?.o test? simple?.o simple? gen.o gen \
              chaos.o chaos quit.o quit replace?.o replace? outOfSwap.o \
              outOfSwap clock.o clock core term[0-3].out

submit: $(CSRCS) $(HDRS) $(TURNIN) 
	tar cvzf phase5.tgz $(CSRCS) $(HDRS) Makefile

