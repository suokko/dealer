#include "ast.h"
#include "operator.h"

namespace ast {

Location::Location(int line, int byte) :
	lineno_(line),
	byte_(byte)
{}

int Location::getLine() const
{
	return lineno_;
}

int Location::getByte() const
{
	return byte_;
}

Node::Node(const Location &loc) :
	loc_(loc)
{}

Node::~Node()
{}

const Location &Node::getLocation() const
{
	return loc_;
}

Expr::Expr(const Location &loc) :
	Node(loc)
{}

NumberExpr::NumberExpr(long value, const Location &loc) :
	Expr(loc),
	val_(value)
{}

long NumberExpr::getValue() const
{
	return val_;
}

void NumberExpr::accept(Visitor &v) const
{
	v.visit(*this);
}

StringExpr::StringExpr(const std::string &data, const ast::Location &loc) :
	Expr(loc),
	data_(data)
{}

void StringExpr::accept(Visitor &v) const
{
	v.visit(*this);
}

const std::string &StringExpr::getText() const
{
	return data_;
}

CallExpr::CallExpr(const std::string &name, const ast::Location &loc) :
	Expr(loc),
	callee_(name)
{}

void CallExpr::accept(Visitor &v) const
{
	v.visit(*this);
}

const std::string &CallExpr::getCallee() const
{
	return callee_;
}

OpExpr::OpExpr(const Operator &op, const Location &loc) :
	Expr(loc),
	op_(op)
{
}

const Operator &OpExpr::getOp() const
{
	return op_;
}

UnaryExpr::UnaryExpr(const Operator &op,std::unique_ptr<ast::Expr> && rhs,
		const Location &loc) :
	OpExpr(op, loc),
	rhs_(std::move(rhs))
{}

void UnaryExpr::accept(Visitor &v) const
{
	v.visit(*this);
}

const ast::Expr &UnaryExpr::getRHS() const
{
	return *rhs_;
}

BinaryExpr::BinaryExpr(const Operator &op, std::unique_ptr<ast::Expr> && lhs,
		std::unique_ptr<ast::Expr> && rhs,
		const Location &loc) :
	OpExpr(op, loc),
	lhs_(std::move(lhs)),
	rhs_(std::move(rhs))
{}

void BinaryExpr::accept(Visitor &v) const
{
	v.visit(*this);
}

const ast::Expr &BinaryExpr::getLHS() const
{
	return *lhs_;
}

const ast::Expr &BinaryExpr::getRHS() const
{
	return *rhs_;
}

TernaryExpr::TernaryExpr(const Operator &op, std::unique_ptr<ast::Expr> && pred,
		std::unique_ptr<ast::Expr> && lhs,
		std::unique_ptr<ast::Expr> && rhs,
		const Location &loc) :
	OpExpr(op, loc),
	pred_(std::move(pred)),
	lhs_(std::move(lhs)),
	rhs_(std::move(rhs))
{}

void TernaryExpr::accept(Visitor &v) const
{
	v.visit(*this);
}

const ast::Expr &TernaryExpr::getPred() const
{
	return *pred_;
}

const ast::Expr &TernaryExpr::getLHS() const
{
	return *lhs_;
}

const ast::Expr &TernaryExpr::getRHS() const
{
	return *rhs_;
}

Prototype::Prototype(const std::string &name, const Location &loc, attrib attribs) :
	Node(loc),
	name_(name),
	attribs_(attribs)
{}

void Prototype::accept(Visitor &v) const
{
	v.visit(*this);
}

const std::string &Prototype::getName() const
{
	return name_;
}

bool Prototype::overwrite() const
{
	return attribs_ == PROTA_OVER;
}

bool Prototype::extend() const
{
	return attribs_ == PROTA_EXTEND;
}

Function::Function(std::unique_ptr<Prototype> && proto, std::unique_ptr<Expr> && e) :
	Expr(e->getLocation()),
	proto_(std::move(proto)),
	body_(std::move(e))
{}

const Prototype &Function::getPrototype() const
{
	return *proto_;
}

const Expr &Function::getBody() const
{
	return *body_;
}

void Function::accept(Visitor &v) const
{
	v.visit(*this);
}

}
