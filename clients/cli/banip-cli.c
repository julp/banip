#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> /* sleep */
// #include <sys/param.h> /* MAXPATHLEN */
// #include <string.h> /* strcpy */

#include "common.h"
#include "banip.h"

int main(int argc, char **argv)
{
	char *error;
	void *client;

	error = NULL;
	if (argc < 2 || argc > 3) {
		printf("expected arguments are: 1) message to send ; 2) socket path\n");
		return EXIT_FAILURE;
	}
    if (NULL == (client = banip_client_new(3 == argc ? argv[2] : NULL, NULL, &error))) {
		fprintf(stderr, "banip_connect: %s\n", error);
		free(error);
	} else {
		int i;

		for (i = 0; i < 32; i++) {
// 			sleep(1);
			if (!banip_ban_from_string(client, S("127.0.0.1"), S("foo"), &error)) {
				fprintf(stderr, "banip_ban_from_string: %s\n", error);
				free(error);
			}
		}
		sleep(3);
		banip_close(client);
	}

	return EXIT_SUCCESS;
}
