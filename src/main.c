/*
 * hotpatch is a dll injection strategy.
 * Copyright (c) 2010-2011, Vikas Naresh Kumar, Selective Intellect LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in the
 *	 documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of Selective Intellect LLC nor the
 *   names of its contributors may be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <hotpatch_config.h>
#include <hotpatch.h>

struct hp_options {
    pid_t pid;
    int verbose;
	bool is__start;
	char *symbol;
	bool dryrun;
};

void print_usage(const char *app)
{
    printf("\nUsage: %s [options] <PID of process to patch>\n", app);
	printf("\nOptions:\n");
	printf("-h           This help message.\n");
	printf("-v[vvvv]     Enable verbose logging. Add more 'v's for more\n");
	printf("-s <name>    Specify symbol name. Default is _start\n");
	printf("-N           Dry run. Do not modify anything in process\n");
}

void print_options(const struct hp_options *opts)
{
	if (opts && opts->verbose > 0) {
		printf(
				"Options Given:\n"
				"Verbose Level: %d\n"
				"Process PID: %d\n"
				"Symbol name: %s\n"
				"Dry run: %s\n",
				opts->verbose,
				opts->pid,
				opts->symbol,
				(opts->dryrun ? "true" : "false")
			  );
	}
}

int parse_arguments(int argc, char **argv, struct hp_options *opts)
{
    if (argc > 0 && argv && opts) {
        int opt = 0;
        extern int optind;
        extern char *optarg;
        optind = 1;
		opts->is__start = false;
		opts->dryrun = false;
        while ((opt = getopt(argc, argv, "hNs:v::")) != -1) {
            switch (opt) {
            case 'v':
                opts->verbose += optarg ? (int)strnlen(optarg, 5) : 1;
                break;
			case 's':
				opts->symbol = strdup(optarg);
				if (!opts->symbol) {
					printf("[%s:%d] Out of memory\n", __func__, __LINE__);
					return -1;
				}
				if (strcmp(optarg, HOTPATCH_LINUX_START) == 0)
					opts->is__start = true;
				else
					opts->is__start = false;
				break;
			case 'N':
				opts->dryrun = true;
				break;
            case 'h':
            default:
                print_usage(argv[0]);
                return -1;
            }
        }
        if (optind >= argc) {
            printf("Expected more arguments.\n");
            print_usage(argv[0]);
            return -1;
        }
        opts->pid = (pid_t)strtol(argv[optind], NULL, 10);
        if (opts->pid == 0) {
            printf("Process PID can't be 0. Tried parsing: %s\n", argv[optind]);
			return -1;
        }
		if (!opts->symbol) {
			opts->symbol = strdup(HOTPATCH_LINUX_START);
			opts->is__start = true;
		}
		if (!opts->symbol) {
			printf("[%s:%d] Out of memory\n", __func__, __LINE__);
			return -1;
		}
        return 0;
    }
    return -1;
}

int main(int argc, char **argv)
{
    struct hp_options opts = { 0 };
    hotpatch_t *hp = NULL;
	int rc = 0;
	/* parse all arguments first */
    if (parse_arguments(argc, argv, &opts) < 0) {
        return -1;
    }
    print_options(&opts);
	/* break from execution whenever a step fails */
	do {
		uintptr_t ptr = 0;
		hp = hotpatch_create(opts.pid, opts.verbose);
		if (!hp) {
			fprintf(stderr, "[%s:%d] Unable to create hotpatch for PID %d\n",
					__func__, __LINE__, opts.pid);
			rc = -1;
			break;
		}
		/* handles the stripped apps as well */
		if (opts.is__start) {
			ptr = hotpatch_get_entry_point(hp);
		} else {
			ptr = hotpatch_read_symbol(hp, opts.symbol, NULL, NULL);
		}
		if (!ptr) {
			printf("Symbol %s not found. Cannot proceed\n", opts.symbol);
			break;
		}
		printf("Symbol %s found at 0x%lx\n", opts.symbol, ptr);
		if (opts.dryrun)
			break;
		rc = hotpatch_inject_library(hp, "libm.so", NULL);
		/*
		rc = hotpatch_attach(hp);
		if (rc < 0) {
			printf("Failed to attach to process. Cannot proceed\n");
			break;
		}
		rc = hotpatch_set_execution_pointer(hp, ptr);
		if (rc < 0) {
			printf("Failed to set execution pointer to 0x%lx\n", ptr);
			break;
		}
		*/
	} while (0);
/*	rc = hotpatch_detach(hp);*/
	hotpatch_destroy(hp);
	hp = NULL;
	if (opts.symbol)
		free(opts.symbol);
    return rc;
}
