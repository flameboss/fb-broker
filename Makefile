NAME = fb-broker
AUTH = auth/auth_fb.c
DEBUG = YES

CFILES = $(shell ls src/*.c test/*.c) $(AUTH)
CXXFILES = $(shell ls src/*.cpp)

UNAME = $(shell uname)

ifeq ($(UNAME), Darwin)
OS_DEPS = /usr/local/bin/autoreconf
OS_DEF = -DHAVE_KQUEUE=1
MYSQL_INC =
PTHREAD_LD =
else
OS_DEPS =/usr/bin/autoreconf
OS_DEF = -DHAVE_EPOLL=1
MYSQL_INC = -I /usr/include/mysql
PTHREAD_LD = -lpthread
endif

BUILD_DIR = $(UNAME)
OFILES = $(addprefix $(BUILD_DIR)/, $(CFILES:.c=.o) $(CXXFILES:.cpp=.o))

OPENSSL_DIR = /usr/local/opt/openssl
OPENSSL_INC_DIR = $(OPENSSL_DIR)/include
OPENSSL_INC = -I $(OPENSSL_INC_DIR)
OPENSSL_LD = -L$(OPENSSL_DIR)/lib -lssl -lcrypto

CFLAGS = $(INC) $(DEF) -Wextra -Werror
INC = -I . -I src -I libconfig/lib -I /usr/local/mysql/include $(OPENSSL_INC) $(MYSQL_INC)
DEF = $(OS_DEF)

ifeq ($(DEBUG), YES)
CFLAGS += -g -O0
DEF += -DCONFIG_ASSERTIONS=1
else
DEF += -DCONFIG_ASSERTIONS=0
endif

LDFLAGS = libconfig/lib/.libs/libconfig.a -L/usr/local/mysql/lib -lmysqlclient $(OPENSSL_LD) $(PTHREAD_LD)

ifeq ($(USER), root)
SUDO =
else
SUDO = sudo
endif


all:	$(OS_DEPS) $(BUILD_DIR)/$(NAME)

info:
	@echo SUDO $(SUDO)

/usr/local/bin/autoreconf:
	brew install automake

/usr/bin/autoreconf:
	$(SUDO) apt-get install -y automake

/usr/bin/makeinfo:
	$(SUDO) apt-get install -y texinfo

run:
	$(BUILD_DIR)/$(NAME)

test::
	time $(BUILD_DIR)/$(NAME) -t

clean:
	rm -rf $(BUILD_DIR)

$(BUILD_DIR)/$(NAME): libconfig $(OFILES)
	$(CXX) $(OFILES) $(LDFLAGS) -o $@

.PHONY: libconfig
libconfig:	libconfig/.libs/libconfig.a

libconfig/.libs/libconfig.a:	libconfig/Makefile
	cd libconfig && make

libconfig/Makefile: libconfig/configure
	cd libconfig && ./configure

libconfig/configure:	libconfig/configure.ac
	cd libconfig && autoreconf

libconfig/configure.ac:
	git submodule update --init --recursive

# Add dependency files, if they exist
-include $(OFILES:.o=.d)

$(BUILD_DIR)/%.o: %.c Makefile
	@mkdir -p $(@D)
	$(CC) -E $(INC) $(DEF) $*.c -o $(@:.o=.p)
	$(CC) $(CFLAGS) -MMD -c $*.c -o $@

$(BUILD_DIR)/%.o: %.cpp Makefile
	@mkdir -p $(@D)
	$(CXX) -E $(INC) $(DEF) $*.cpp -o $(@:.o=.p)
	$(CXX) $(CFLAGS) -MMD -c $*.cpp -o $@
