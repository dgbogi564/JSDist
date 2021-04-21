#ifndef P2_COMPARE_H
#define P2_COMPARE_H

#ifndef BUFSIZE
#define BUFSIZE 256
#endif

#ifndef MAX_WAIT_TIME_IN_SECONDS
#define MAX_WAIT_TIME_IN_SECONDS (2)
#endif

#ifndef MAX_WAIT_TIME_IN_NANOSECONDS
#define MAX_WAIT_TIME_IN_NANOSECONDS (0*1000000000)
#endif

/* usage: compare {File ...|Directory...} [-dN] [-fN] [-aN] [-sS]
	N: positive integers
	S: file name suffix
Recursively obtains all files of the extension "txt" (or from the specified extension)
from the directories and files given and calculates each pair's Jensen-Shannon distance.

-dN:	specifies the # of directory threads used
		(default # of directory threads: 1)

-fN:	specifies the # of file threads used
		(default # of file threads: 1)

-aN:	specifies the # of analysis threads used
		(default # of analysis threads: 1)

-sS:	specifies the file name suffix
		(default file suffix: txt)
*/


void usage() {
	printf(	"usage: compare {File ...|Directory...} [%c[1m-d%c[0mN] [%c[1m-f%c[0mN] [%c[1m-a%c[0mN] [%c[1m-s%c[0mS]\n"
	            "\tN: positive integers\n"
	            "\tS: file name suffix\n"
	            "Recursively obtains all files of the extension \"txt\" (or from the specified extension)\n"
	            "from the directories and files given and calculates each pair's Jensen-Shannon distance.\n\n"
	            "-dN:\tspecifies the # of directory threads used\n"
	            "\t\t(default # of directory threads: 1)\n\n"
	            "-fN:\tspecifies the # of file threads used\n"
	            "\t\t(default # of file threads: 1)\n\n"
	            "-aN:\tspecifies the # of analysis threads used\n"
	            "\t\t(default # of analysis threads: 1)\n\n"
	            "-sS:\tspecifies the file name suffix\n"
	            "\t\t(default file suffix: txt)\n",
	            27, 27, 27, 27, 27, 27, 27, 27);
	exit(EXIT_FAILURE);
}

#endif //P2_COMPARE_H
