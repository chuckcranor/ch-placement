/*
 * Copyright (C) 2013 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/time.h>

#include "oid-gen.h"
#include "ch-placement.h"

struct options
{
    unsigned int num_servers;
    unsigned int num_objs;
    unsigned int replication;
    char* placement;
    unsigned int virt_factor;
};

static int usage (char *exename);
static struct options *parse_args(int argc, char *argv[]);

#define OBJ_ARRAY_SIZE 5000000UL

static double Wtime(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return((double)t.tv_sec + (double)(t.tv_usec) / 1000000);
}

int main(
    int argc,
    char **argv)
{
    struct options *ig_opts = NULL;
    unsigned long total_byte_count = 0;
    unsigned long total_obj_count = 0;
    struct obj* total_objs = NULL;
    unsigned int i;
    double t1, t2;
    struct ch_placement_instance *instance;

    ig_opts = parse_args(argc, argv);
    if(!ig_opts)
    {
        usage(argv[0]);
        return(-1);
    }

    instance = ch_placement_initialize(ig_opts->placement, 
        ig_opts->num_servers,
        ig_opts->virt_factor);

    /* generate random set of objects for testing */
    printf("# WARNING: remember to compile this with -O2 at least.\n");
    printf("# Generating random object IDs...\n");
    oid_gen("random", instance, OBJ_ARRAY_SIZE, ULONG_MAX,
        8675309, ig_opts->replication, ig_opts->num_servers,
        NULL,
        &total_byte_count, &total_obj_count, &total_objs);
    printf("# Done.\n");

    assert(total_obj_count == OBJ_ARRAY_SIZE);

    sleep(1);

    printf("# Running placement benchmark...\n");
    /* run placement benchmark */
    t1 = Wtime();
#pragma omp parallel for
    for(i=0; i<ig_opts->num_objs; i++)
    {
        ch_placement_find_closest(instance, total_objs[i%OBJ_ARRAY_SIZE].oid, ig_opts->replication, total_objs[i%OBJ_ARRAY_SIZE].server_idxs);
    }
    t2 = Wtime();
    printf("# Done.\n");

    printf("# <objects>\t<replication>\t<servers>\t<algorithm>\t<time (s)>\t<rate oids/s>\n");
    printf("%u\t%d\t%u\t%s\t%f\t%f\n",
        ig_opts->num_objs,
        ig_opts->replication,
        ig_opts->num_servers,
        ig_opts->placement,
        t2-t1,
        (double)ig_opts->num_objs/(t2-t1));

    /* we don't need the global list any more */
    free(total_objs);
    total_obj_count = 0;
    total_byte_count = 0;

    return(0);
}

static int usage (char *exename)
{
    fprintf(stderr, "Usage: %s [options]\n", exename);
    fprintf(stderr, "    -s <number of servers>\n");
    fprintf(stderr, "    -o <number of objects>\n");
    fprintf(stderr, "    -r <replication factor>\n");
    fprintf(stderr, "    -p <placement algorithm>\n");
    fprintf(stderr, "    -v <virtual nodes per physical node>\n");

    exit(1);
}

static struct options *parse_args(int argc, char *argv[])
{
    struct options *opts = NULL;
    int ret = -1;
    int one_opt = 0;

    opts = (struct options*)malloc(sizeof(*opts));
    if(!opts)
        return(NULL);
    memset(opts, 0, sizeof(*opts));

    while((one_opt = getopt(argc, argv, "s:o:r:hp:v:")) != EOF)
    {
        switch(one_opt)
        {
            case 's':
                ret = sscanf(optarg, "%u", &opts->num_servers);
                if(ret != 1)
                    return(NULL);
                break;
            case 'o':
                ret = sscanf(optarg, "%u", &opts->num_objs);
                if(ret != 1)
                    return(NULL);
                break;
            case 'v':
                ret = sscanf(optarg, "%u", &opts->virt_factor);
                if(ret != 1)
                    return(NULL);
                break;
            case 'r':
                ret = sscanf(optarg, "%u", &opts->replication);
                if(ret != 1)
                    return(NULL);
                break;
            case 'p':
                opts->placement = strdup(optarg);
                if(!opts->placement)
                    return(NULL);
                break;
            case '?':
                usage(argv[0]);
                exit(1);
        }
    }

    if(opts->replication < 2)
        return(NULL);
    if(opts->num_servers < (opts->replication+1))
        return(NULL);
    if(opts->num_objs < 1)
        return(NULL);
    if(opts->virt_factor < 1)
        return(NULL);
    if(!opts->placement)
        return(NULL);

    assert(opts->replication <= CH_MAX_REPLICATION);

    return(opts);
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ft=c ts=8 sts=4 sw=4 expandtab
 */