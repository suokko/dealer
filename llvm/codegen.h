#pragma once

#include "ast.h"
#include <memory>

namespace llvm {
	class LLVMContext;
	class Module;
	class ExecutionEngine;
	class IRBuilderBase;
	class Value;
	class Function;
}

namespace ast {

/**
 * Class to generate llvm IR from AST
 */
class ASTPrinter : public Visitor {
	std::string indent_;
	bool newline_;
public:
	ASTPrinter();

	void visit(const NumberExpr &node);
	void visit(const StringExpr &node);
	void visit(const CallExpr &node);
	void visit(const UnaryExpr &node);
	void visit(const BinaryExpr &node);
	void visit(const TernaryExpr &node);
	void visit(const Prototype &node);
	void visit(const Function &node);
};

class CompileContext {
	llvm::LLVMContext *context_;
	llvm::Module *module_;
	llvm::ExecutionEngine *engine_;
	llvm::IRBuilderBase *builder_;
public:
	/**
	 * Create compile context that use llvm  global Context
	 */
	CompileContext();

	~CompileContext();

	llvm::LLVMContext &getContext();
	llvm::Module &getModule();
	llvm::IRBuilderBase &getBuilder();
	llvm::ExecutionEngine &getEngine();
};

class Codegen : public Visitor {
	CompileContext *context_;
	llvm::Function *func_;
	llvm::Value *rv_;
protected:
	llvm::Value *error(const char *str);
public:
	Codegen(CompileContext &ctx);

	void visit(const NumberExpr &node);
	void visit(const StringExpr &node);
	void visit(const CallExpr &node);
	void visit(const OpExpr &node);
	void visit(const UnaryExpr &node);
	void visit(const BinaryExpr &node);
	void visit(const TernaryExpr &node);
	void visit(const Prototype &node);
	void visit(const Function &node);

	llvm::Function *getResult();

	llvm::Value *getReturn() const noexcept;
};

}
