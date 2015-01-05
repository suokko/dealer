#pragma once

struct prngtable {
	const unsigned char *table;
	struct entry {
		unsigned int idx;
		unsigned int mask;
	} entries[51];
};

extern const struct prngtable prnglookup;
