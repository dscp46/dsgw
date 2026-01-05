#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "dextra.h"

void sig_handler( int signum);
int setup_signal(int signo, void (*handler)(int));
dextra_server_args_t *srv_args;

int main( int argc, char *argv[], char *envp[])
{
	int ret;
	appconf_t cfg;
	if(!(ret = parse_cmdline_args( argc, argv, &cfg)))
		return ret;

	setup_signal( SIGINT, sig_handler); 

	dextra_server_args_t server_args;
	dextra_server_args_init( &server_args);
	srv_args = &server_args;

	if ( argc > 1 )
		server_args.addr = argv[1];

	dextra_server( &server_args);

	return 0;
}

void sig_handler( int signum)
{
	switch( signum)
	{
	case SIGINT:
		fprintf( stderr, "\n");
		srv_args->shutdown = 1;
		break;

	default:
		;
	}
}	

int setup_signal(int signo, void (*handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);

    return sigaction(signo, &sa, NULL);
}
