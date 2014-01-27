#pragma once

class Lexer;
class ParserPrivate;

class Parser {
	ParserPrivate *p;
public:
	Parser(Lexer *lxr);
	~Parser();

	void parse();
};
