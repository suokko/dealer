#pragma once

#include <string>
#include <memory>

namespace llvm {
	class Function;
	class Value;
}

namespace ast {

class Operator;
class Visitor;

/**
 * Stores the start of token
 */
class Location {
	int lineno_;
	int byte_;
public:
	Location(int lineno, int byte);

	int getLine() const;
	int getByte() const;
};

/**
 * Base class for all ast nodes
 */
class Node {
	Location loc_;
public:
	Node(const Location &loc);
	virtual ~Node();

	const Location &getLocation() const;

	virtual void accept(Visitor &visitor) const = 0;
};

/**
 * Base class for code expressions
 */
class Expr : public Node {
public:
	Expr(const Location &loc);
};

/**
 * Number expression is only integer
 */
class NumberExpr : public Expr {
	long val_;
public:
	NumberExpr(long value, const Location &loc);

	virtual void accept(Visitor &visitor) const;

	long getValue() const;
};

/**
 * Holds a literal string.
 */
class StringExpr : public Expr {
	std::string data_;
public:
	StringExpr(const std::string &data, const Location &loc);

	virtual void accept(Visitor &visitor) const;

	const std::string &getText() const;
};

/**
 * Function call - no parameters in dealer
 */
class CallExpr : public Expr {
	std::string callee_;
public:
	CallExpr(const std::string &callee, const Location &loc);

	virtual void accept(Visitor &visitor) const;

	const std::string &getCallee() const;
};

/**
 * Parent class for all operators
 */
class OpExpr : public Expr {
	const Operator &op_;
public:
	OpExpr(const Operator &op, const Location &loc);

	virtual void accept(Visitor &visitor) const = 0;

	const Operator &getOp() const;
};

/**
 * Unary operator
 */
class UnaryExpr : public OpExpr {
	std::unique_ptr<ast::Expr> rhs_;

public:
	UnaryExpr(const Operator &op, std::unique_ptr<ast::Expr> && rhs,
			const Location &loc);

	virtual void accept(Visitor &visitor) const;

	const ast::Expr &getRHS() const;
};

/**
 * Binary operator
 */
class BinaryExpr : public OpExpr {
	std::unique_ptr<ast::Expr> lhs_, rhs_;

public:
	BinaryExpr(const Operator &op, std::unique_ptr<ast::Expr> && lhs,
			std::unique_ptr<ast::Expr> && rhs,
			const Location &loc);

	virtual void accept(Visitor &visitor) const;

	const ast::Expr &getLHS() const;
	const ast::Expr &getRHS() const;
};

/**
 * Ternary operator
 */
class TernaryExpr : public OpExpr {
	std::unique_ptr<ast::Expr> pred_, lhs_, rhs_;

public:
	TernaryExpr(const Operator &op, std::unique_ptr<ast::Expr> && pred,
			std::unique_ptr<ast::Expr> && lhs,
			std::unique_ptr<ast::Expr> && rhs,
			const Location &loc);

	virtual void accept(Visitor &visitor) const;

	const ast::Expr &getPred() const;
	const ast::Expr &getLHS() const;
	const ast::Expr &getRHS() const;
};

class Function;

/**
 * Function type information
 */
class Prototype : public Node {
public:
	enum attrib {
		PROTA_NONE,
		PROTA_OVER,
		PROTA_EXTEND,
	};
private:
	std::string name_;
	attrib attribs_;
public:
	Prototype(const std::string &name, const Location &loc, attrib attribs = PROTA_NONE);

	virtual void accept(Visitor &visitor) const;

	const std::string &getName() const;
	bool overwrite() const;
	bool extend() const;
};

/**
 * Function defination
 */
class Function : public Expr {
	std::unique_ptr<Prototype> proto_;
	std::unique_ptr<Expr> body_;
public:
	Function(std::unique_ptr<Prototype> && proto, std::unique_ptr<Expr> && body);

	virtual void accept(Visitor &visitor) const;

	const Prototype &getPrototype() const;
	const Expr &getBody() const;
};

/**
 * Base class for visitor implementations
 */
class Visitor {
public:
	virtual void visit(const NumberExpr &node) = 0;
	virtual void visit(const StringExpr &node) = 0;
	virtual void visit(const CallExpr &node) = 0;
	virtual void visit(const UnaryExpr &node) = 0;
	virtual void visit(const BinaryExpr &node) = 0;
	virtual void visit(const TernaryExpr &node) = 0;

	virtual void visit(const Prototype &node) = 0;
	virtual void visit(const Function &node) = 0;
};

}
