#include "lexer.h"
#include "parser.h"
#include <llvm/Support/Debug.h>

int main(void)
{
//	llvm::DebugFlag = 1;
	Lexer lxr;
	Parser p(&lxr);
	p.parse();
	return 0;
}
