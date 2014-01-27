#pragma once

#include <memory>

class Lexer;
class ParserPrivate;

namespace llvm {
class Value;
}

namespace ast {

class Expr;
class CompileContext;
class Codegen;

class Keyword {
	std::string name_;
protected:
	Keyword(const std::string &name);
public:
	virtual ~Keyword();
	/**
	 * Translate keyword to matching token in lexer
	 */
	virtual int translate() const noexcept;
	/**
	 * Parse they keyword to as node
	 */
	virtual std::unique_ptr<Expr> parse(ParserPrivate *parser, std::unique_ptr<Expr> && lhs) const noexcept;
	/**
	 * Generate code for the keyword
	 */
	virtual llvm::Value *generate(CompileContext &ctx,
				      Codegen &visit,
				      const Expr &node) const noexcept;

	const std::string &getName() const noexcept;
	static const Keyword *factory(const std::string &identifier) noexcept;
};
}
