
#include "operator.h"
#include "codegen.h"
#include "ast.h"

#include <map>
#include <memory>
#include <algorithm>
#include <cassert>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>

#define MAKEOP1(t1) (t1 << 8)
#define MAKEOP2(t1, t2) ((t1 << 8) | t2)

namespace {

struct OperatorCmp {
	bool operator() (const int &a, const int &b) const noexcept
	{
		const int bottom = 0xFF;
		if (matchHighOnly)
			return (a & ~bottom) < (b & ~bottom);
		return a < b;
	}
private:
	static bool matchHighOnly;
	friend class ast::Operator;
};

typedef std::unique_ptr<ast::Operator> opptr;
typedef std::map<int, opptr, OperatorCmp> opset;
bool OperatorCmp::matchHighOnly = false;

static opset operators;

class NullOperator : public ast::Operator {
public:
	NullOperator(int op) noexcept :
		ast::Operator(op, -1, 0)
	{}

	llvm::Value *generate(ast::CompileContext &, ast::Codegen &, const ast::OpExpr &) const noexcept
	{ assert(!"NullOperator no allowed"); return nullptr; }
};

NullOperator nullOperator(0);

}

namespace ast {

template <typename T>
static bool operator==(const std::pair<const int, T> &a, const int &b)
{
	return a.first == b;
}

Operator::Operator(int op, int prec, int direction, int paramcount, int term) noexcept :
	op_(op),
	term_(term),
	precedence_(prec),
	direction_(direction),
	paramcount_(paramcount)
{
#ifndef NDEBUG
	int mask = 0xffff << 16;
#endif
	assert(direction == 1 || direction == 0);
	assert((op & mask) == 0 && "No support for three or more bytes per operator");
}

Operator::~Operator()
{
}

int Operator::getOp() const noexcept
{
	return op_;
}

int Operator::getPrecedence() const noexcept
{
	return precedence_ != 0x1f ? precedence_ : -1;
}

int Operator::getParamCount() const noexcept
{
	return paramcount_;
}

int Operator::getDirection() const noexcept
{
	return direction_ ? 1 : -1;
}

int Operator::getTerminator() const noexcept
{
	return term_;
}

int Operator::bytes() const noexcept
{
	return (op_ & 0xff) == 0 ? 1 : 2;
}

const Operator &Operator::factory(int tok, int nexttok) noexcept
{
	OperatorCmp::matchHighOnly = true;
	auto iterpair = operators.equal_range(MAKEOP1(tok));
	OperatorCmp::matchHighOnly = false;
	if (iterpair.first == iterpair.second)
		return nullOperator;

	auto iter = std::find(iterpair.first, iterpair.second, MAKEOP2(tok, nexttok));
	if (iter == iterpair.second) {
		/* Return the single character operator if no match for next byte made */
		if (iterpair.first->second->bytes() == 1)
			return *iterpair.first->second;
		else
			return nullOperator;
	}

	return *iter->second;
};

const Operator &Operator::factory(int tok) noexcept
{
	auto iter = operators.find(tok);
	if (iter == operators.end())
		return nullOperator;

	return *iter->second;
}

bool Operator::needPeek(int tok) noexcept
{
	OperatorCmp::matchHighOnly = true;
	auto iterpair = operators.equal_range(MAKEOP1(tok));
	OperatorCmp::matchHighOnly = false;
	if (iterpair.first == iterpair.second)
		return false;

	if (iterpair.first == --iterpair.second)
		return (iterpair.first->second->getOp() & 0xff) != 0;

	return true;
}

}

namespace {
llvm::IRBuilder<> &b(ast::CompileContext &ctx)
{
	return static_cast<llvm::IRBuilder<> &>(ctx.getBuilder());
}

class ArithmeticOperator : public ast::Operator {
public:
	ArithmeticOperator(int op, int prec) noexcept :
		ast::Operator(op, prec, 1)
	{}

	llvm::Value *generate(ast::CompileContext &c, ast::Codegen &visit, const ast::OpExpr &op) const noexcept
	{
		const ast::BinaryExpr &node = static_cast<const ast::BinaryExpr &>(op);
		node.getLHS().accept(visit);
		llvm::Value *lhs = visit.getReturn();
		node.getRHS().accept(visit);
		llvm::Value *rhs = visit.getReturn();
		switch (getOp()) {
		case MAKEOP1('+'): return b(c).CreateNSWAdd(lhs, rhs, "addtmp");
		case MAKEOP1('-'): return b(c).CreateNSWSub(lhs, rhs, "subtmp");
		case MAKEOP1('*'): return b(c).CreateNSWMul(lhs, rhs, "multmp");
		case MAKEOP1('/'): return b(c).CreateSDiv(lhs, rhs, "divtmp");
		case MAKEOP1('%'): return b(c).CreateSRem(lhs, rhs, "remtmp");
		case MAKEOP1('&'): return b(c).CreateAnd(lhs, rhs, "andtmp");
		case MAKEOP1('|'): return b(c).CreateOr(lhs, rhs, "ortmp");
		case MAKEOP1('^'): return b(c).CreateXor(lhs, rhs, "xortmp");
		case MAKEOP2('<','<'): return b(c).CreateShl(lhs, rhs, "shltmp");
		case MAKEOP2('>','>'): return b(c).CreateAShr(lhs, rhs, "shltmp");
		}
		return nullptr;
	}
};

class LogicalOperator : public ast::Operator {
public:
	LogicalOperator(int op, int prec) noexcept :
		ast::Operator(op, prec, 1)
	{}

	llvm::Value *generate(ast::CompileContext &c, ast::Codegen &visit, const ast::OpExpr &op) const noexcept
	{
		const ast::BinaryExpr &node = static_cast<const ast::BinaryExpr &>(op);
		/* Generate lhs block */
		node.getLHS().accept(visit);
		llvm::Value *lhs = visit.getReturn();
		/* Convert values to bool */
		llvm::Value *l = b(c).CreateICmpNE(lhs, b(c).getInt64(0), "lhstobool");

		/* Create conditional blocks for short circuit */
		llvm::Function *func = b(c).GetInsertBlock()->getParent();
		llvm::BasicBlock *lBB = b(c).GetInsertBlock();
		llvm::BasicBlock *rBB = llvm::BasicBlock::Create(c.getContext(), "logicrhs", func);
		llvm::BasicBlock *eBB = llvm::BasicBlock::Create(c.getContext(), "logicend");

		/* Generate branch to rhs computation */
		switch (getOp()) {
		case MAKEOP2('&','&'):
			b(c).CreateCondBr(l, rBB, eBB);
			break;
		case MAKEOP2('|','|'):
			b(c).CreateCondBr(l, eBB, rBB);
			break;
		default:
			assert(!"Impossible conditional operator");
		}
		b(c).SetInsertPoint(rBB);
		/* generate code for RHS expression */
		node.getRHS().accept(visit);
		llvm::Value *rhs = visit.getReturn();
		llvm::Value *r = b(c).CreateICmpNE(rhs, b(c).getInt64(0), "rhstobool");
		/* Jump to the end block */
		b(c).CreateBr(eBB);
		/* Update the insert block if rhs generated new blocks */
		rBB = b(c).GetInsertBlock();
		/* Generate end block to combine values */
		func->getBasicBlockList().push_back(eBB);
		b(c).SetInsertPoint(eBB);
		llvm::PHINode *PN = b(c).CreatePHI(b(c).getInt1Ty(), 2, "logicphi");
		/* Add PHI node branch incoming values */
		PN->addIncoming(l, lBB);
		PN->addIncoming(r, rBB);
		return  b(c).CreateIntCast(PN, b(c).getInt64Ty(), false, "castlogicop");
	}
};

class CmpOperator : public ast::Operator {
public:
	CmpOperator(int op, int prec) noexcept :
		ast::Operator(op, prec, 1)
	{}

	llvm::Value *generate(ast::CompileContext &c, ast::Codegen &visit, const ast::OpExpr &op) const noexcept
	{
		const ast::BinaryExpr &node = static_cast<const ast::BinaryExpr &>(op);
		node.getLHS().accept(visit);
		llvm::Value *lhs = visit.getReturn();
		node.getRHS().accept(visit);
		llvm::Value *rhs = visit.getReturn();
		switch (getOp()) {
		case MAKEOP1('<'): return b(c).CreateIntCast(b(c).CreateICmpSLT(lhs, rhs, "lttmp"), b(c).getInt64Ty(), false, "castlt");
		case MAKEOP1('>'): return b(c).CreateIntCast(b(c).CreateICmpSGT(lhs, rhs, "gttmp"), b(c).getInt64Ty(), false, "castgt");
		case MAKEOP2('<', '='): return b(c).CreateIntCast(b(c).CreateICmpSLE(lhs, rhs, "lttmp"), b(c).getInt64Ty(), false, "castle");
		case MAKEOP2('>', '='): return b(c).CreateIntCast(b(c).CreateICmpSGE(lhs, rhs, "gttmp"), b(c).getInt64Ty(), false, "castge");
		case MAKEOP2('=','='): return b(c).CreateIntCast(b(c).CreateICmpEQ(lhs, rhs, "eqtmp"), b(c).getInt64Ty(), false, "casteq");
		case MAKEOP2('!','='): return b(c).CreateIntCast(b(c).CreateICmpNE(lhs, rhs, "netmp"), b(c).getInt64Ty(), false, "castne");
		}
		return nullptr;
	}
};

class UnaryOperator : public ast::Operator {
public:
	UnaryOperator(int op, int prec) noexcept :
		ast::Operator(op, prec, 0, 1)
	{}

	llvm::Value *generate(ast::CompileContext &c, ast::Codegen &visit, const ast::OpExpr &op) const noexcept
	{
		const ast::UnaryExpr &node = static_cast<const ast::UnaryExpr &>(op);
		node.getRHS().accept(visit);
		llvm::Value *rhs = visit.getReturn();
		switch (getOp()) {
		case MAKEOP2('-','\1'): return b(c).CreateMul(rhs, b(c).getInt64(-1), "unaryminus");
		case MAKEOP2('+','\1'): return rhs;
		case MAKEOP2('!','\1'): return b(c).CreateIntCast(
							b(c).CreateICmpEQ(rhs, b(c).getInt64(0), "unarylogicnot"),
							b(c).getInt64Ty(), false, "castlogicnot");
		case MAKEOP2('~','\1'): return b(c).CreateNot(rhs, "unarynot");
		}
		return nullptr;
	}
};

class TernaryOperator : public ast::Operator {
public:
	TernaryOperator(int op, int prec) noexcept :
		ast::Operator(op & 0xffff, prec, 0, 3, op >> 16)
	{
	}

	llvm::Value *generate(ast::CompileContext &c, ast::Codegen &visit, const ast::OpExpr &op) const noexcept
	{
		const ast::TernaryExpr &node = static_cast<const ast::TernaryExpr &>(op);

		node.getPred().accept(visit);
		/* Generate predicate block */
		llvm::Value *pred = visit.getReturn();
		pred = b(c).CreateICmpNE(pred, b(c).getInt64(0), "predtobool");

		/* Setup blocks */
		llvm::Function *func = b(c).GetInsertBlock()->getParent();
		llvm::BasicBlock *lBB = llvm::BasicBlock::Create(c.getContext(), "ternarylhs", func);
		llvm::BasicBlock *rBB = llvm::BasicBlock::Create(c.getContext(), "ternaryrhs");
		llvm::BasicBlock *eBB = llvm::BasicBlock::Create(c.getContext(), "ternaryend");

		switch (getOp()) {
		case MAKEOP1('?'):
			b(c).CreateCondBr(pred, lBB, rBB);
			break;
		default: return nullptr;
		}

		/* Generate lhs block */
		b(c).SetInsertPoint(lBB);
		node.getLHS().accept(visit);
		llvm::Value *lhs = visit.getReturn();
		b(c).CreateBr(eBB);
		lBB = b(c).GetInsertBlock();

		/* Generate rhs block */
		func->getBasicBlockList().push_back(rBB);
		b(c).SetInsertPoint(rBB);
		node.getRHS().accept(visit);
		llvm::Value *rhs = visit.getReturn();
		b(c).CreateBr(eBB);
		rBB = b(c).GetInsertBlock();

		/* Generate end block to combine values */
		func->getBasicBlockList().push_back(eBB);
		b(c).SetInsertPoint(eBB);
		llvm::PHINode *PN = b(c).CreatePHI(b(c).getInt64Ty(), 2, "ternaryphi");
		PN->addIncoming(lhs, lBB);
		PN->addIncoming(rhs, rBB);

		return PN;
	}
};

struct RegisterOperators {
	template<typename T>
	static void construct(int tok, int prec) noexcept
	{
#ifndef NDEBUG
		auto r =
#endif
			operators.emplace(std::piecewise_construct,
				std::forward_as_tuple(tok & 0xffff),
				std::forward_as_tuple(new T(tok, prec)));
		assert(r.second);
	}

	RegisterOperators() noexcept
	{
		int tok, prec;
		/* Unary has to be in start of expression or after binary operator */
		tok = MAKEOP2('+','\1');
		prec = 22;
		construct<UnaryOperator>(tok, prec);
		tok = MAKEOP2('-','\1');
		prec = 22;
		construct<UnaryOperator>(tok, prec);
		tok = MAKEOP2('!','\1');
		prec = 22;
		construct<UnaryOperator>(tok, prec);
		tok = MAKEOP2('~','\1');
		prec = 22;
		construct<UnaryOperator>(tok, prec);
		/* Binary operators after primary expression */
		tok = MAKEOP1('*');
		prec = 20;
		construct<ArithmeticOperator>(tok, prec);
		tok = MAKEOP1('/');
		prec = 20;
		construct<ArithmeticOperator>(tok, prec);
		tok = MAKEOP1('%');
		prec = 20;
		construct<ArithmeticOperator>(tok, prec);
		tok = MAKEOP1('+');
		prec = 18;
		construct<ArithmeticOperator>(tok, prec);
		tok = MAKEOP1('-');
		prec = 18;
		construct<ArithmeticOperator>(tok, prec);
		tok = MAKEOP2('<','<');
		prec = 16;
		construct<ArithmeticOperator>(tok, prec);
		tok = MAKEOP2('>','>');
		prec = 16;
		construct<ArithmeticOperator>(tok, prec);
		tok = MAKEOP1('<');
		prec = 14;
		construct<CmpOperator>(tok, prec);
		tok = MAKEOP1('>');
		prec = 14;
		construct<CmpOperator>(tok, prec);
		tok = MAKEOP2('<', '=');
		prec = 14;
		construct<CmpOperator>(tok, prec);
		tok = MAKEOP2('>', '=');
		prec = 14;
		construct<CmpOperator>(tok, prec);
		tok = MAKEOP2('=','=');
		prec = 12;
		construct<CmpOperator>(tok, prec);
		tok = MAKEOP2('!','=');
		prec = 12;
		construct<CmpOperator>(tok, prec);
		tok = MAKEOP1('&');
		prec = 10;
		construct<ArithmeticOperator>(tok, prec);
		tok = MAKEOP1('^');
		prec = 8;
		construct<ArithmeticOperator>(tok, prec);
		tok = MAKEOP1('|');
		prec = 6;
		construct<ArithmeticOperator>(tok, prec);
		tok = MAKEOP2('&', '&');
		prec = 4;
		construct<LogicalOperator>(tok, prec);
		tok = MAKEOP2('|', '|');
		prec = 4;
		construct<LogicalOperator>(tok, prec);
		tok = MAKEOP1('?') | ':' << 16;
		prec = 2;
		construct<TernaryOperator>(tok, prec);
	}
} registerOperators;
}
