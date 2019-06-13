#MISS_TARGET_LIST = gva32
MISS_TARGET_LIST =
SO_C_SRCS_ORG = $(wildcard src/*.c)
SO_CPP_SRCS_ORG = $(wildcard src/*.cpp)
SO_C_SRCS = $(notdir $(SO_C_SRCS_ORG))
SO_CPP_SRCS = $(notdir $(SO_CPP_SRCS_ORG))
SO_OBJS_MID = $(SO_C_SRCS:.c=.o) $(SO_CPP_SRCS:.cpp=.o)  
SO_OBJS = $(filter-out $(SO_REMOVE_OBJ),$(SO_OBJS_MID))

SRC_SRCS_ORG = src/easyipc_daemon.c src/dlist.c src/easyipc_debug.c src/easyipc_misc.c src/easyipc_plugin.c src/easyipc_script.c src/cjson.c src/easyipc_conf_support.c
SRC_SRCS = $(notdir $(SRC_SRCS_ORG))
SRC_OBJS = $(patsubst %.c,%.o,$(SRC_SRCS))

SO_SRCS_ORG = src/easyipc_support.c src/dlist.c src/easyipc_backtrace.c  src/easyipc_conf_support.c
SO_SRCS = $(notdir $(SO_SRCS_ORG))
SO_OBJS = $(patsubst %.c,%.o,$(SO_SRCS))

CLI_SRCS_ORG = src/easyipc_console.c
CLI_SRCS = $(notdir $(CLI_SRCS_ORG))
CLI_OBJS = $(patsubst %.c,%.o,$(CLI_SRCS))

CLI_OBJ = eipc
SO_OBJ  = libeipc.so
EXE_OBJ = eipcd


CFLAGS += -I./include -I./exp_inc

ifneq ("$(toolsysroots)", "")
	CFLAGS+= --sysroot=$(toolsysroots)
endif

selfdir=$(shell basename $(shell pwd))
ifeq ("$(DIR)", "")
	DIR=$(selfdir)
endif
ifeq ("$(BUILDDIR)", "")
	BUILDDIR="build"
endif
ifeq ("$(PREFIX)","")
	PREFIX=out/$(targetname)
endif

LDFLAGS += -L. -L$(PREFIX)/lib -L$(THIS_BUILD_DIR)/


THIS_BUILD_DIR=$(BUILDDIR)/$(DIR)/

all:init $(SO_OBJ) $(EXE_OBJ) $(CLI_OBJ) 
init:
	-mkdir -p $(THIS_BUILD_DIR)
	@echo "$(SRC_OBJS)"

$(EXE_OBJ):$(SRC_OBJS) 
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(THIS_BUILD_DIR)/$@ $(patsubst %,$(BUILDDIR)/$(DIR)/%,$^) -I. -lrt -lpthread -O2 -Werror -lm 

$(SO_OBJ):$(SO_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o $(THIS_BUILD_DIR)/$@ $(patsubst %,$(BUILDDIR)/$(DIR)/%,$^) -I. -lrt -lpthread -O2 -Werror 

$(CLI_OBJ):$(CLI_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(THIS_BUILD_DIR)/$@ $(patsubst %,$(BUILDDIR)/$(DIR)/%,$^) -I. -lrt -lpthread -leipc -O2 -Werror 

%.o : src/%.c
	$(CC) $(CFLAGS) -fpic -c $< -o $(BUILDDIR)/$(DIR)/$@ -I. -O2 

demo:
	@echo "************************** nothing ******************************"
install:
	install -d -m0755 $(PREFIX)/bin
	install -d -m0755 $(PREFIX)/lib
	install -d -m0755 $(PREFIX)/include
	rm -rf $(PREFIX)/bin/eipc*
	install -m0755 $(THIS_BUILD_DIR)/$(SO_OBJ) $(PREFIX)/lib
	install -m0755 $(THIS_BUILD_DIR)/$(CLI_OBJ) $(PREFIX)/bin
	install -m0755 $(THIS_BUILD_DIR)/$(EXE_OBJ) $(PREFIX)/bin
	install -m0755 $(THIS_BUILD_DIR)/$(RPC_OBJ) $(PREFIX)/bin


	cp ./exp_inc/*.h $(tonly_base_inc)
	cp ./exp_inc/*.h $(PREFIX)/include
	@ cd $(PREFIX)/bin/ && ln -s $(CLI_OBJ) eipcmsg && ln -s $(CLI_OBJ) eipcprint && ln -s $(CLI_OBJ) eipcls && ln -s $(CLI_OBJ) eipccat

.PHONY:clean creat
clean:
	@echo "删除所有$(BUILDDIR)/$(DIR)"
	-rm -rf $(BUILDDIR)/$(DIR)
	-rm -rf out
ifneq ("$(PREFIX)","")
	-rm -rf $(PREFIX)
endif

creat:$(toolchainlist)
	@echo "创建环境"
	-mkdir -p exp_inc include src out
	@echo "version:? ">README
$(toolchainlist):
	-mkdir -p out/$@/include out/$@/lib out/$@/bin	

