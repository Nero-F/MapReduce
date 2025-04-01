#include "common.h"
#include "linked_list.h"
#include "coordinator.h"
#include <bits/getopt_core.h>
#include <getopt.h>
#include <locale.h>
#include <stdbool.h>

void display_help(void)
{
    printf("Usage: coordinator -f|--files [files] [options(s)]\n");
    printf("\t-P, --port\trunning port, (default: %d)\n", DEFAULT_RUNNING_PORT);
    printf("\t-f, --files\tinput files\n");
    printf("\t-n, --nreduce\tnumber of partitions, (default: %d)\n",
        DEFAULT_NREDUCE);
    printf("\t-h, --help\tdisplay this help list\n");
}

static int parse_arg(const int ac, char *const av[], coordinator_t *coord)
{
    bool is_files_specified = false;
    static struct option opts[] = {
        { "port", required_argument, NULL, 'P' },
        { "files", required_argument, NULL, 'f' },
        { "nreduce", required_argument, NULL, 'R' }, // number of partitions
        { "help", no_argument, NULL, 'h' },
    };
    int opt_idx = 0;
    char ret_opt = 0;

    while ((ret_opt = getopt_long(ac, av, "P:f:R:h", opts, &opt_idx)) != -1) {
        switch (ret_opt) {
            case 'P':
                ATOI_ARG(coord->running_port, optarg, av[optind - 1]);
                printf("[%d]\n", coord->running_port);
                break;
            case 'f':
                is_files_specified = true;
                --optind;
                files_t files = { 0 };
                for (; optind < ac && *av[optind] != '-'; optind++) {
#ifdef DEBUG
                    if (access(av[optind], R_OK) == -1) {
                        perror("access");
                        return FAILURE;
                    }
#endif
                    da_append(&files, av[optind]);
                }
                coord->files = &files;
                break;
            case 'R':
                ATOI_ARG(coord->n_reduce, optarg, av[optind - 1]);
                break;
            case 'h':
                display_help();
                exit(SUCCESS);
            default:
                return FAILURE;
        }
    }

    if (is_files_specified == false) {
        fprintf(stderr,
            "You must specified -f or --files argument. See -h for more "
            "informations.\n");
        return FAILURE;
    }
    return SUCCESS;
}

static llist_t init_tasks(task_type_t type, uint size)
{
    llist_t task_list = NULL;

    machine_t m = {
        .id = -1,
        .state = IDLE,
    };

    for (uint i = 0; i < size; i++) {
        task_t *t = malloc(sizeof(task_t));
        ASSERT_MEM(t);
        t->id = i;
        t->worker = m;
        t->type = type;
        t->thread = NULL;
        assert(list_add_elem_at_back(&task_list, t) != false
            && "could not add to tasklist\n");
    }
    return task_list;
}

static void deinint_coord(coordinator_t *coord)
{
    if (coord->files) free(coord->files->items);
    if (coord->map_task) list_clear(&coord->map_task, true);
    if (coord->reduce_task) list_clear(&coord->reduce_task, true);
}

int main(const int ac, char *const av[])
{
    int ret_val = SUCCESS;
    coordinator_t coord = { 0 };
    coord.running_port = DEFAULT_RUNNING_PORT;
    coord.n_reduce = DEFAULT_NREDUCE;
    coord.pinger_running = false;

    if (parse_arg(ac, av, &coord) == FAILURE)
        ret_val = FAILURE;
    else {
        coord.map_task = init_tasks(MAP, coord.files->count);
        coord.n_map = coord.files->count;
        coord.reduce_task = init_tasks(REDUCE, coord.n_reduce);
        ret_val = run_server(coord);
    }
    deinint_coord(&coord);

    return ret_val;
}
