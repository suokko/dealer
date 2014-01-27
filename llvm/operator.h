#pragma once

namespace llvm {
	class Value;
}

namespace ast {

class CompileContext;
class OpExpr;
class Codegen;

class Operator {
	unsigned op_ : 16;
	unsigned term_ : 8;
	unsigned precedence_ : 5;
	unsigned direction_ : 1;
	unsigned paramcount_ : 2;
protected:
	Operator(int op, int precedence, int direction, int paramcount = 2, int term = 0) noexcept;
public:
	virtual ~Operator() noexcept;

	virtual llvm::Value *generate(CompileContext &ctx,
				      Codegen &visit,
				      const OpExpr &node) const noexcept = 0;

	int getOp() const noexcept;
	int getPrecedence() const noexcept;
	int getDirection() const noexcept;
	int getParamCount() const noexcept;
	int getTerminator() const noexcept;
	int bytes() const noexcept;

	/**
	 * Fetch the operator matching the token
	 */
	static const Operator &factory(int tok, int nexttok) noexcept;
	static const Operator &factory(int tok) noexcept;
	/**
	 * Check if this token needs to peek next token for two character operator
	 */
	static bool needPeek(int tok) noexcept;
};
}
