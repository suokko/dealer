#include "codegen.h"
#include "operator.h"
#include <memory>
#include <cassert>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#pragma GCC diagnostic  pop
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>

namespace ast {

ASTPrinter::ASTPrinter() :
	newline_(true)
{}

void ASTPrinter::visit(const StringExpr &node)
{
	if (newline_) {
		fprintf(stderr, "%s", indent_.c_str());
		newline_ = false;
	}
	fprintf(stderr, "\"%s\"", node.getText().c_str());
}

void ASTPrinter::visit(const CallExpr &node)
{
	if (newline_) {
		fprintf(stderr, "%s", indent_.c_str());
		newline_ = false;
	}
	fprintf(stderr, "%s()", node.getCallee().c_str());
}

void ASTPrinter::visit(const NumberExpr &node)
{
	if (newline_) {
		fprintf(stderr, "%s", indent_.c_str());
		newline_ = false;
	}
	fprintf(stderr, "%ld", node.getValue());
}

void ASTPrinter::visit(const Prototype &node)
{
	if (newline_) {
		fprintf(stderr, "%s", indent_.c_str());
		newline_ = false;
	}
	fprintf(stderr, "int %s()\n", node.getName().c_str());
	newline_ = true;
}

void ASTPrinter::visit(const UnaryExpr &node)
{
	if (newline_) {
		fprintf(stderr, "%s", indent_.c_str());
		newline_ = false;
	}
	fprintf(stderr, "(");
	fprintf(stderr, "%c", node.getOp().getOp() >> 8);
	node.getRHS().accept(*this);
	fprintf(stderr, ")");
}

void ASTPrinter::visit(const BinaryExpr &node)
{
	if (newline_) {
		fprintf(stderr, "%s", indent_.c_str());
		newline_ = false;
	}
	fprintf(stderr, "(");
	node.getLHS().accept(*this);
	if (node.getOp().bytes() == 1)
		fprintf(stderr, " %c ", node.getOp().getOp() >> 8);
	else
		fprintf(stderr, " %c%c ", node.getOp().getOp() >> 8, node.getOp().getOp());
	node.getRHS().accept(*this);
	fprintf(stderr, ")");
}

void ASTPrinter::visit(const TernaryExpr &node)
{
	if (newline_) {
		fprintf(stderr, "%s", indent_.c_str());
		newline_ = false;
	}
	fprintf(stderr, "(");
	fprintf(stderr, "(");
	node.getPred().accept(*this);
	fprintf(stderr, ")");
	if (node.getOp().bytes() == 1)
		fprintf(stderr, " %c ", node.getOp().getOp() >> 8);
	else
		fprintf(stderr, " %c%c ", node.getOp().getOp() >> 8, node.getOp().getOp());
	fprintf(stderr, "(");
	node.getLHS().accept(*this);
	fprintf(stderr, ")");
	fprintf(stderr, " %c ", node.getOp().getTerminator());
	fprintf(stderr, "(");
	node.getRHS().accept(*this);
	fprintf(stderr, ")");
	fprintf(stderr, ")");
}

void ASTPrinter::visit(const Function &node)
{
	if (newline_) {
		fprintf(stderr, "%s", indent_.c_str());
		newline_ = false;
	}
	node.getPrototype().accept(*this);
	fprintf(stderr, "{\n");
	indent_ += "  ";
	newline_ = true;
	node.getBody().accept(*this);
	indent_.resize(indent_.size() - 2);
	fprintf(stderr, "\n%s}\n", indent_.c_str());
	newline_ = true;
}

CompileContext::CompileContext() :
	context_(&llvm::getGlobalContext()),
	module_(new llvm::Module("Automatic Module", *context_)),
	builder_(new llvm::IRBuilder<>(*context_))
{
	llvm::InitializeNativeTarget();
	std::string err;
	engine_ = llvm::EngineBuilder(module_).setErrorStr(&err).create();
	if (!engine_) {
		fprintf(stderr, "Engine building failed: %s\n", err.c_str());
		exit(2);
	}
	assert(engine_ != 0);
}

CompileContext::~CompileContext()
{
	delete engine_;
	delete builder_;
}

llvm::LLVMContext &CompileContext::getContext()
{
	return *context_;
}

llvm::IRBuilderBase &CompileContext::getBuilder()
{
	return *builder_;
}

llvm::Module &CompileContext::getModule()
{
	return *module_;
}

llvm::ExecutionEngine &CompileContext::getEngine()
{
	return *engine_;
}

Codegen::Codegen(CompileContext &ctx) :
	context_(&ctx),
	func_(0),
	rv_(0)
{}

llvm::Function *Codegen::getResult()
{
	return func_;
}

llvm::Value *Codegen::error(const char *str)
{
	fprintf(stderr, "%s\n", str);
	return 0;
}

llvm::IRBuilder<> &b(llvm::IRBuilderBase &builder)
{
	return static_cast<llvm::IRBuilder<> &>(builder);
}

#define return(x) {rv_ = (x); return;}

void Codegen::visit(const StringExpr &node)
{
	(void)node;
#if 1
	return(nullptr);
#else
	return(llvm::ConstantArray::get(llvm::ArrayType::get(
					llvm::Type::getInt8Ty(context_->getContext()),
					node.getText().length()), node.getText().c_str()));
#endif
}

void Codegen::visit(const CallExpr &node)
{
	llvm::Function *callee = context_->getModule().getFunction(node.getCallee());

	if (!callee)
		return(error("Unknown function referenced\n"));

	return(b(context_->getBuilder()).CreateCall(callee));
}

void Codegen::visit(const NumberExpr &node)
{
	return(llvm::ConstantInt::get(context_->getContext(),
				llvm::APInt(64, node.getValue(), true)));
}

void Codegen::visit(const Prototype &node)
{
	llvm::FunctionType *ft = llvm::FunctionType::get(llvm::Type::getInt64Ty(context_->getContext()), false);

	llvm::Function *f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
						   node.getName(), &context_->getModule());

	if (f->getName() != node.getName()) {
		f->eraseFromParent();
		f = context_->getModule().getFunction(node.getName());

		if (!f->empty()) {
			if (node.overwrite()) {
				f->deleteBody();
			} else if (!node.extend()) {
				return(error("redefination of function"));
			}
			this->context_->getEngine().freeMachineCodeForFunction(f);
		}
	}
	return(f);
}

void Codegen::visit(const OpExpr &node)
{
	llvm::Value *rv = node.getOp().generate(*context_, *this, node);
	if (!rv)
		return(error("Operator generation failed\n"));
	return(rv);
}

void Codegen::visit(const UnaryExpr &node)
{
	visit(static_cast<const OpExpr &>(node));
}

void Codegen::visit(const BinaryExpr &node)
{
	visit(static_cast<const OpExpr &>(node));
}

void Codegen::visit(const TernaryExpr &node)
{
	visit(static_cast<const OpExpr &>(node));
}

llvm::Value *Codegen::getReturn() const noexcept
{
	return rv_;
}

#undef return
#define return(x) {func_ = (x); return;}

void Codegen::visit(const Function &node)
{
	assert(rv_ == nullptr);
	node.getPrototype().accept(*this);
	llvm::Function *f = static_cast<llvm::Function *>(rv_);
	if (!f)
		return(f);

	llvm::BasicBlock *bb;
	if (f->empty()) {
		bb = llvm::BasicBlock::Create(context_->getContext(), "entry", f);
	} else {
		bb = &f->getBasicBlockList().back();
		bb->back().removeFromParent();
	}
	b(context_->getBuilder()).SetInsertPoint(bb);

	node.getBody().accept(*this);
	if (!rv_) {
		f->eraseFromParent();
		return(nullptr);
	}

	b(context_->getBuilder()).CreateRet(rv_);
	return(f);
}

}
