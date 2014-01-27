#pragma once

#include <stdio.h>
#include <string>

enum Token {
	tok_eof		= -1,

	// commands
	tok_function	= -2,

	// primary
	tok_identifier	= -3,
	tok_number	= -4,
	tok_string	= -5,
	tok_keyword	= -6,
};

namespace ast {
class Keyword;
}

class LexerPrivate;

class Lexer {
	class LexerPrivate *p;
public:
	/**
	 * Read tokens from stdin
	 */
	Lexer();
	/**
	 * Read tokens from given stream
	 */
	Lexer(FILE *stream);

	~Lexer();

	/**
	 * Fetch cached current token
	 */
	int getToken() const;
	long getValue() const;
	const std::string &getName() const;
	const ast::Keyword &getKeyword() const;
	/**
	 * Line where token started
	 */
	int getTokenLine() const;
	/**
	 * Byte offset inside line where token starts
	 */
	int getTokenByte() const;

	/**
	 * Process the next Token
	 */
	int getNextToken();
};
