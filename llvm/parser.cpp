#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "codegen.h"
#include "operator.h"
#include "keyword.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#pragma GCC diagnostic pop
#include <llvm/ExecutionEngine/GenericValue.h>

#include <memory>
#include <unordered_map>

namespace {

class FunctionChecker : public ast::Visitor {
	bool toplevel_;
	bool isfunction_;
	const ast::Function *func_;

public:
	FunctionChecker() : toplevel_(true), isfunction_(false), func_(nullptr) {}

	void visit(const ast::NumberExpr &)
	{
		toplevel_ = false;
	}
	void visit(const ast::StringExpr &)
	{
		toplevel_ = false;
	}
	void visit(const ast::CallExpr &)
	{
		toplevel_ = false;
	}
	void visit(const ast::UnaryExpr &node)
	{
		toplevel_ = false;
		node.getRHS().accept(*this);
	}
	void visit(const ast::BinaryExpr &node)
	{
		toplevel_ = false;
		node.getLHS().accept(*this);
		node.getRHS().accept(*this);
	}
	void visit(const ast::TernaryExpr &node)
	{
		toplevel_ = false;
		node.getPred().accept(*this);
		node.getLHS().accept(*this);
		node.getRHS().accept(*this);
	}
	void visit(const ast::Prototype &)
	{
		toplevel_ = false;
	}
	void visit(const ast::Function &node)
	{
		if (!toplevel_) {
			/* Error */
			func_ = &node;
			return;
		}
		isfunction_ = true;
		toplevel_ = false;
		node.getPrototype().accept(*this);
		node.getBody().accept(*this);
	}

	const ast::Function *getError() const {return func_;}
	bool isFunction() const {return isfunction_;}
};
}

struct ParserPrivate {
	Lexer *lxr_;
	ast::CompileContext ctx_;

	ParserPrivate(Lexer *lxr) : lxr_(lxr)
	{}

	~ParserPrivate()
	{
		ctx_.getModule().dump();
	}

	ast::Location getLocation() const
	{
		ast::Location loc(lxr_->getTokenLine(), lxr_->getTokenByte());
		return loc;
	}

	std::unique_ptr<ast::Expr> Error(const char *str) const
	{
		std::unique_ptr<ast::Expr> r;
		fprintf(stderr, "%d: %s\n", lxr_->getTokenLine(), str);
		return r;
	}

	std::unique_ptr<ast::Expr> Error(const char *str, const ast::Expr *node) const
	{
		std::unique_ptr<ast::Expr> r;
		fprintf(stderr, "%d: %s\n", node->getLocation().getLine(), str);
		return r;
	}

	std::unique_ptr<ast::Expr> ParseNumber()
	{
		ast::Location loc = getLocation();
		std::unique_ptr<ast::Expr> r(
			new ast::NumberExpr(lxr_->getValue(), loc));
		lxr_->getNextToken();
		return r;
	}

	std::unique_ptr<ast::Expr> ParseString()
	{
		ast::Location loc = getLocation();
		std::unique_ptr<ast::Expr> r(
			new ast::StringExpr(lxr_->getName(), loc));
		lxr_->getNextToken();
		return r;
	}

	int GetTokPrecedence() const
	{
		switch (lxr_->getToken()) {
		case '<':
		case '>':
			return 10;
		case '+':
		case '-':
			return 20;
		case '*':
		case '/':
			return 40;
		default:
			return -1;
		}
	}

	std::unique_ptr<ast::Expr> ParseBinOp(int prec, std::unique_ptr<ast::Expr> lhs)
	{
		while (1) {
			int tok = lxr_->getToken();
			const ast::Operator &binop = ast::Operator::factory(tok);
			int tokprec = binop.getPrecedence();
			if (tokprec < prec)
				return lhs;

			ast::Location loc = getLocation();
			lxr_->getNextToken();
			std::unique_ptr<ast::Expr> lhsval;

			if (binop.getParamCount() == 3) {
				lhsval = ParseExpression();
				if (lxr_->getToken() != binop.getTerminator())
					return Error("Expected ':'");
				lxr_->getNextToken();
			}

			std::unique_ptr<ast::Expr> rhs = ParsePrimary();
			if (!rhs)
				return rhs;

			rhs = ParseBinOp(tokprec+binop.getDirection(), std::move(rhs));
			if (!rhs)
				return rhs;

			if (binop.getParamCount() == 3)
				lhs.reset(new ast::TernaryExpr(binop, std::move(lhs), std::move(lhsval), std::move(rhs), loc));
			else
				lhs.reset(new ast::BinaryExpr(binop, std::move(lhs), std::move(rhs), loc));
		}
	}

	std::unique_ptr<ast::Expr> ParseUnaryOp(int prec)
	{
		std::unique_ptr<ast::Expr> rhs;
		while (1) {
			int tok = lxr_->getToken();
			/* Unary operators have 1 as prefix always */
			if (tok < 256)
				tok <<= 8;
			const ast::Operator &binop = ast::Operator::factory(tok | 1);
			int tokprec = binop.getPrecedence();
			if (tokprec < prec)
				return rhs;

			ast::Location loc = getLocation();
			lxr_->getNextToken();

			rhs = ParsePrimary(tokprec + binop.getDirection());
			if (!rhs)
				return Error("Unknown token when expecting an expression");
			rhs.reset(new ast::UnaryExpr(binop, std::move(rhs), loc));
		}
	}

	std::unique_ptr<ast::Expr> ParseIdentifier()
	{
		ast::Location loc = getLocation();
		std::string name = lxr_->getName();

		if (lxr_->getNextToken() != '=') {
			std::unique_ptr<ast::Expr> r(new ast::CallExpr(name, loc));
			return r;
		}

		/* Eat = */
		lxr_->getNextToken();

		std::unique_ptr<ast::Prototype> proto(new ast::Prototype(name, loc));

		std::unique_ptr<ast::Expr> body = ParseExpression();
		if (!body)
			return body;

		std::unique_ptr<ast::Expr> r(new ast::Function(std::move(proto), std::move(body)));
		return r;
	}

	std::unique_ptr<ast::Expr> ParseParentheses()
	{
		/* Eat ( */
		lxr_->getNextToken();
		std::unique_ptr<ast::Expr> r(ParseExpression());
		if (!r)
			return r;

		if (lxr_->getToken() != ')')
			return Error("Expected ')'");
		/* Eat c */
		lxr_->getNextToken();
		return r;
	}

	std::unique_ptr<ast::Expr> ParsePrimary(int prec = 0)
	{
		switch (lxr_->getToken()) {
		default:
			return ParseUnaryOp(prec);
		case tok_number:
			return ParseNumber();
		case tok_string:
			return ParseString();
		case tok_identifier:
			return ParseIdentifier();
		case '(':
			return ParseParentheses();
		}
	}


	std::unique_ptr<ast::Expr> ParseExpression()
	{
		if (lxr_->getToken() == tok_keyword)
			return lxr_->getKeyword().parse(this, nullptr);
		std::unique_ptr<ast::Expr> lhs = ParsePrimary();
		if (!lhs) return lhs;

		std::unique_ptr<ast::Expr> op = ParseBinOp(0, std::move(lhs));
		return op;
	}

	std::unique_ptr<ast::Expr> ParseTopLevelExpr()
	{
		std::unique_ptr<ast::Expr> e(ParseExpression());
		if (!e)
			return 0;

		FunctionChecker checker;
		e->accept(checker);

		if (checker.getError())
			return Error("Function defined inside another function.", checker.getError());

		if (checker.isFunction())
			return e;

		/* Create an anonymous function for top level expression */
		std::unique_ptr<ast::Prototype> proto(new ast::Prototype("", e->getLocation()));
		std::unique_ptr<ast::Expr> f(
				new ast::Function(std::move(proto), std::move(e)));
		return f;
	}

	void HandleTopLevelExpression()
	{
		std::unique_ptr<ast::Expr> f(ParseTopLevelExpr());
		if (!f) {
			fprintf(stderr, "Top level parsing failed!\n");
			lxr_->getNextToken();
			return;
		}

		ast::ASTPrinter printer;

		f->accept(printer);

		ast::Codegen gen(ctx_);

		f->accept(gen);

		if (!gen.getResult()) {
			fprintf(stderr, "Compilation failed\n");
			return;
		}

		gen.getResult()->dump();

		llvm::ExecutionEngine &eng = ctx_.getEngine();

		std::vector<llvm::GenericValue> args;
		llvm::GenericValue r = eng.runFunction(gen.getResult(), args);

		fprintf(stderr, "Results is %s\n",r.IntVal.toString(10, true).c_str());

		return;
	}
};

Parser::Parser(Lexer *lxr) : p(new ParserPrivate(lxr))
{}

Parser::~Parser()
{
	delete p;
}

void Parser::parse()
{
	fprintf(stderr, "ready> ");
	p->lxr_->getNextToken();
	while (1) {
		fprintf(stderr, "ready> ");
		switch (p->lxr_->getToken()) {
		case tok_eof:
			return;
		case ';':
			p->lxr_->getNextToken();
			break;
		default:
			p->HandleTopLevelExpression();
			break;
		}
	}
}

/* keyword.cpp has to to be part of parser to allow calling ParserPrivate */

namespace ast {

typedef std::unique_ptr<Keyword> keywordptr;
typedef std::unordered_map<std::string, keywordptr> keywordmap;

static keywordmap keywords;

Keyword::Keyword(const std::string &name) :
	name_(name)
{}

Keyword::~Keyword()
{}

int Keyword::translate() const noexcept
{
	return tok_keyword;
}

const std::string &Keyword::getName() const noexcept
{
	return name_;
}

const Keyword *Keyword::factory(const std::string &identifier)
{
	auto iter = keywords.find(identifier);
	if (iter == keywords.end())
		return nullptr;
	return iter->second.get();
}

std::unique_ptr<ast::Expr> Keyword::parse(ParserPrivate *, std::unique_ptr<ast::Expr> &&) const noexcept
{
	abort();
	return nullptr;
}

llvm::Value *Keyword::generate(ast::CompileContext &, ast::Codegen &, const ast::Expr &) const noexcept
{
	abort();
	return nullptr;
}

}

namespace {

class AndKeyword : public ast::Keyword {
public:
	AndKeyword() noexcept :
		Keyword("and")
	{}

	int translate() const noexcept
	{
		return '&' << 8 | '&';
	}
};

class OrKeyword : public ast::Keyword {
public:
	OrKeyword() noexcept :
		Keyword("or")
	{}

	int translate() const noexcept
	{
		return '|' << 8 | '|';
	}
};

class NotKeyword : public ast::Keyword {
public:
	NotKeyword() noexcept :
		Keyword("not")
	{}

	int translate() const noexcept
	{
		return '!' << 8;
	}
};

class FnKeyword : public ast::Keyword {
public:
	FnKeyword(const std::string &name) :
		Keyword(name)
	{}

	std::unique_ptr<ast::Expr> parse(ParserPrivate *parser, std::unique_ptr<ast::Expr> &&) const noexcept
	{
		ast::Location loc = parser->getLocation();

		parser->lxr_->getNextToken();

		std::unique_ptr<ast::Prototype> proto(new ast::Prototype(getName(), loc, ast::Prototype::PROTA_OVER));

		std::unique_ptr<ast::Expr> body = parser->ParseExpression();
		if (!body)
			return body;

		std::unique_ptr<ast::Expr> r(new ast::Function(std::move(proto), std::move(body)));
		return r;
	}
};

class PredealKeyword : public ast::Keyword {
public:
	PredealKeyword() :
		Keyword("predeal")
	{}

	std::unique_ptr<ast::Expr> parse(ParserPrivate *parser, std::unique_ptr<ast::Expr> &&) const noexcept
	{
		ast::Location loc = parser->getLocation();

		parser->lxr_->getNextToken();

		std::unique_ptr<ast::Prototype> proto(new ast::Prototype(getName(), loc, ast::Prototype::PROTA_EXTEND));

		std::unique_ptr<ast::Expr> body = parser->ParseExpression();
		if (!body)
			return body;

		std::unique_ptr<ast::Expr> r(new ast::Function(std::move(proto), std::move(body)));
		return r;
	}
};

struct RegisterKeywords {
	static void insert(ast::Keyword *kw) noexcept
	{
		ast::keywords.insert(std::make_pair(kw->getName(), ast::keywordptr(kw)));
	}

	RegisterKeywords() noexcept
	{
		insert(new AndKeyword());
		insert(new OrKeyword());
		insert(new NotKeyword());

		insert(new FnKeyword("generate"));
		insert(new FnKeyword("produce"));
		insert(new FnKeyword("condition"));
		insert(new FnKeyword("dealer"));
		insert(new FnKeyword("vulnerable"));

		insert(new PredealKeyword());
	}
} registerkeywords;
}
