#ifndef LANG
#define LANG

#include "parser.h"
#include "compiler.h"

class Call : public ParserNode {
public:
	Call(bool c=false) : ParserNode(c){}
	use_ext_GRAMMAR;
	ValueRef* getValue(bool dropRet);
	
	~Call() {}
};

class MemLabel : public ParserNode {
public:
	MemLabel(bool c=false) : ParserNode(c){}
	GRAMMAR(
		{new Token("&"), new AlphaStr(true)}
	)
	ValueRef* getValue();

	~MemLabel() {}
};

class Term : public ParserNode {
public:
	Term(bool c=false) : ParserNode(c){}
	use_ext_GRAMMAR
	ValueRef* getValue();

	~Term() {}
};

class BinaryOP : public ParserNode {
public:
	BinaryOP(bool c=false) : ParserNode(c){}
	GRAMMAR(
		{new Token("+", true), new Term(true)},
		{new Token("-", true), new Term(true)}
	)
	Term* RightHandSide() {
		return (Term*)children[1];	
	}
	std::string Operation() {
		return ((Token*)children[0])->token;
	}
	~BinaryOP() {}
}; 

class Expr : public ParserNode {
public:
	Expr(bool c=false) : ParserNode(c){}
	GRAMMAR(
		{new Term(true), new Any<BinaryOP>(true)}
	)
	ValueRef* getValue();
	~Expr() {}
};

class VarLabel : public ParserNode {
public:
	VarLabel(bool c=false) : ParserNode(c){}
	GRAMMAR(
		{new AlphaStr(true)},
		{new Token("*"), new VarLabel(true)}
	)
	bool isDeref();
	void unlock();
	void lock();
	void setValue(ValueRef* val);
	ValueRef* getValue();
	~VarLabel() {}
};

class Assignment : public ParserNode {
public:
	Assignment(bool c=false) : ParserNode(c){}
	GRAMMAR(
		{new VarLabel(true), new Token("="), new Expr(true)}
	)
	void Assemble();
	
	~Assignment() {}
};

class Declaration : public ParserNode {
public:
	Declaration(bool c=false) : ParserNode(c){}
	GRAMMAR(
		{new Token("var"), new AlphaStr(true)}
	)
	void Assemble();
	
	~Declaration() {}
};

class If : public ParserNode {
public:
	If(bool c=false) : ParserNode(c){}
	use_ext_GRAMMAR;
	void Assemble();
	
	~If() {}
};

class While : public ParserNode {
public:
	While(bool c=false) : ParserNode(c){}
	use_ext_GRAMMAR;
	void Assemble();
	
	~While() {}
};

class Return : public ParserNode {
public:
	Return(bool c=false) : ParserNode(c){}
	GRAMMAR(
		{new Token("ret"), new Expr(true)}
	)
	void Assemble();
	
	~Return() {}
};

class Statement : public ParserNode {
public:
	Statement(bool c=false) : ParserNode(c){}
	use_ext_GRAMMAR;
	void Assemble();
	
	~Statement() {}
};

class CodeBlock : public ParserNode {
public:
	CodeBlock(bool c=false) : ParserNode(c){}
	GRAMMAR(
		{new Token("{"), new Any<Statement>(true), new Token("}")}
	)
	std::vector<ParserNode*> getStatements() {
		return children[0]->children;
	}
	void Assemble();
	~CodeBlock() {}
};

class Function : public ParserNode {
public:
	Function(bool c=false) : ParserNode(c){}
	GRAMMAR(
		{new Token("fn"), new AlphaStr(true), new Token("("), new CommaSeparated<AlphaStr>(true), new Token(")"), new CodeBlock(true)}
	)
	std::string getName() {
		return ((AlphaStr*)children[0])->text;
	}
	void Assemble();
	~Function() {}
};

class Program : public ParserNode {
public:
	Program(bool c=false) : ParserNode(c){}
	GRAMMAR(
		{new Any<Function>(true)}
	)
	void Assemble();
	~Program() {}
};

#endif