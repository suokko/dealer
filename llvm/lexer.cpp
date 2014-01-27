#include "lexer.h"
#include "operator.h"
#include "keyword.h"
#include <cctype>
#include <string>

struct LexerPrivate {
	FILE *stream_;
	int token_;
	int toklineno_;
	int tokbyte_;
	int last_;
	int next_;
	int lineno_;
	int byte_;
	std::string name_;
	long value_;
	const ast::Keyword *keyword_;

	LexerPrivate(FILE *stream) :
		stream_(stream),
		token_(tok_eof),
		toklineno_(-1),
		tokbyte_(-1),
		last_(' '),
		next_(' '),
		lineno_(0),
		byte_(-1)
	{}

	int peek() {
		if (next_ == -1)
			next_ = getc(stream_);
		return next_;
	}

	int get()
	{
		int c = peek();
		next_ = -1;
		if (c == '\n') {
			lineno_++;
			byte_ = 0;
		} else {
			byte_++;
		}
		return c;
	}

	void storeTokenPosition()
	{
		toklineno_ = lineno_;
		tokbyte_ = byte_;
	}

	int skipcomment(const char *end) {
		int pos = 0;
		while ((last_ = get()) != EOF) {
			if (end[pos] == '\0')
				break;
			if (last_ == end[pos])
				pos++;
			else
				pos = 0;
		}

		if (end[0] != '\n') {
			// Report error for multiline comment ending to EOF
		}

		return getToken();
	}

	int parseIdentifier()
	{
		while (std::isalnum(last_ = get()))
			name_ += last_;

		keyword_ = ast::Keyword::factory(name_);
		if (keyword_)
			return keyword_->translate();

		return tok_identifier;
	}

	int parseString()
	{
		int start = last_;
		while ((last_ = get()) != start) {
			if (last_ == '\\')
				last_ = get();
			if (last_ == EOF) {
				// Report error for string ending to EOF
				break;
			}
			name_ += last_;
		}
		if (last_ == start)
			last_ = get();
		return tok_string;
	}

	int parseNumber()
	{
		std::string num;
		do {
			num += last_;
			last_ = get();
		} while (std::isdigit(last_));

		if (std::isalpha(last_)) {
			name_ = num;
			name_ += last_;
			return parseIdentifier();
		}

		value_ = atol(num.c_str());
		return tok_number;
	}

	int getToken()
	{
		// Skip whitespace
		while(std::isspace(last_))
			last_ = get();

		// Skip comments
		switch (last_) {
		case '/':
			if (peek() == '*') {
				last_ = get();
				return skipcomment("*/");
			}
			if (peek() == '/') {
				last_ = get();
				return skipcomment("\n");
			}
			break;
		case '#':
			return skipcomment("\n");
		}

		storeTokenPosition();

		// Parse a string
		if (last_ == '"' || last_ == '\'')
			return parseString();

		// Parse an identifier
		if (std::isalpha(last_)) {
			name_ = last_;
			return parseIdentifier();
		}

		// Parse number
		if (std::isdigit(last_))
			return parseNumber();

		if (last_ == EOF)
			return tok_eof;

		// Parse single or two character operator
		int next = 0;
		if (ast::Operator::needPeek(last_))
			next = peek();
		const ast::Operator &op = ast::Operator::factory(last_, next);
		if (op.getPrecedence() >= 0) {
			last_ = get();
			if (op.bytes() == 2)
				last_ = get();
			return op.getOp();
		}

		int cur = last_;
		last_ = get();
		return cur;
	}
};

Lexer::Lexer() : p(new LexerPrivate(stdin))
{
}

Lexer::Lexer(FILE *stream) : p(new LexerPrivate(stream))
{
}

Lexer::~Lexer()
{
	delete p;
}

int Lexer::getToken() const
{
	return p->token_;
}

int Lexer::getTokenLine() const
{
	return p->toklineno_;
}

int Lexer::getTokenByte() const
{
	return p->tokbyte_;
}

long Lexer::getValue() const
{
	return p->value_;
}

const std::string & Lexer::getName() const
{
	return p->name_;
}

int Lexer::getNextToken()
{
	p->token_ = p->getToken();
	return p->token_;
}

const ast::Keyword &Lexer::getKeyword() const
{
	return *p->keyword_;
}
