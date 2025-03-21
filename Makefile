BLUE	=		\033[36m
WHITE	=		\033[0m

WRKR_SRC	=	src/worker.c			\
			src/plug_loading.c	

COORD_SRC	=	src/coordinator.c 		\
			src/linked_list.c 		\
			src/network/frpc.c		\
			src/network/coord_event_loop.c

WRKR_OBJS	=	$(WRKR_SRC:%.c=%.o)
COORD_OBJS	=	$(COORD_SRC:%.c=%.o)

PLUG 	=	plug

CFLAGS	=	-W -Wall -Wextra -g -O3 -fshort-enums #-Werror 
CPPFLAGS =	-I./include/

WORKER	=	worker
COORDINATOR	=	coordinator

all: $(WORKER) $(COORDINATOR)

$(PLUG): ## build plug.so
	$(CC) ./src/$@.c -fpic -shared -o $@.so

$(COORDINATOR): $(COORD_OBJS) ## build coordinator binary
	$(CC) $^ -o $@ $(CFLAGS) -DPOLLER -DDEBUG

$(WORKER): $(WRKR_OBJS) ## build worker binary
	$(CC) $^ -o $@ $(CFLAGS)

run_coord: $(COORDINATOR) ## build and run coordinator
	./$^ --leak-check=full --track-origins=yes

run_worker: $(WORKER) $(PLUG) ## build and run worker
	./$< ./$(PLUG).so

clean: ## deleting objects files
	$(RM) $(WRKR_OBJS) $(COORD_OBJS) vgcore.*

fclean: clean ## deleting binaries and shared object
	$(RM) $(WORKER) $(COORDINATOR) $(PLUG).so

re: fclean all

help:
	@printf "USAGE:\n\n"
	@grep  -E "^[a-zA-Z\$$\(\)_]+:.*##" $(MAKEFILE_LIST) | sed "s/.*/\L&/;s/[\$$()]/\L/g" | awk -F ":.*## " '{ printf "$(WHITE)\t%-25s$(BLUE)%s\n", $$1, $$2}'

.PHONY: all clean fclean re
