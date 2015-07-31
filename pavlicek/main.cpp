#include "ghp.h"
#include "shp.h"
#include "gsx.h"
#include "handpattern.h"
#include <unistd.h>
#include <cstdlib>
#include <iostream>


template<class T>
static void printgenerichand()
{
	unsigned i;
	T generic;

	for (i = 0; i < generic.size(); i++) {
		auto value = generic[i];
		std::cout << value << '\n';
	}
}

static int help(const char *prog, int r)
{
	std::cout << " " << prog << " <options>\n\n"
		"\t-g\t<type>\tPrint generic <hand> patterns\n"
		;
	std::exit(r);
}

static int dogeneric(const char *prog, const char *arg)
{
	if (arg[0] == 'h')
		printgenerichand<GHP>();
	else if (arg[0] == 's')
		printgenerichand<SHP>();
	else if (arg[0] == 'x')
		printgenerichand<GSX>();
	else
		help(prog, 2);
	return 0;
}

int main(int argc, char * const* argv)
{
	int opt;
	while((opt = getopt(argc, argv, "g:")) != -1) {
		switch(opt) {
		case 'g':
			dogeneric(argv[0], optarg);
			break;
		default:
			help(argv[0], 1);
		}
	}
	return 0;
}
