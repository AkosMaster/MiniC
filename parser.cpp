#include <vector>
#include <iostream>
#include <typeinfo>
#include <ctype.h>
#include <string>

//#include "memtrace.h"
//#include "gtest_lite.h"
#define GRAMMAR(...) std::vector<std::vector<ParserNode*>> grammar(){return {__VA_ARGS__};};
#define use_ext_GRAMMAR std::vector<std::vector<ParserNode*>> grammar();
#define set_ext_GRAMMAR(c, ...) std::vector<std::vector<ParserNode*>> c::grammar(){return {__VA_ARGS__};}

int alloc_count = 0;
int node_id = 0;

class StackVar {
public:
	int scope = 0;
	int stackDepth = 0;
	std::string name = "";
	StackVar(int s, std::string n, int st) {scope = s; name = n; stackDepth = st;}
};

#define CACHE false

class Compiler {
	#define REG_COUNT 14
	bool cached[16] = {false};
	std::string regCache[16] = {""};
	bool usedRegs[REG_COUNT] = {false};

	void cacheReg(int reg, std::string val) {
		regCache[reg] = val;
		cached[reg] = true;
	}
	
	int scope = 0;
	int stackDepth = 0;

	std::vector<StackVar> vars;
public:
	std::string _asm;
	
	std::string getReg(int reg) {
		clearCache(reg);
		return "r" + std::to_string(reg);
	}
	std::string getRegCached(int reg) {
		if (cached[reg]) {
			return regCache[reg];
		}
		return "r" + std::to_string(reg);
	}
	void clearCache(int reg) {
		if (cached[reg]) {
			std::string cached_val = regCache[reg];
			cached[reg] = false;
			_asm += "mov " + getReg(reg) + ", " + cached_val + "\n";
		}
	}
	
	void op_add(int to, int from) {
		if (to == -1) {return;}
		clearCache(to);
		_asm += "add " + getReg(to) + ", " + getRegCached(from) + "\n";
	}

	void op_sub(int to, int from) {
		if (to == -1) {return;}
		clearCache(to);
		_asm += "sub " + getReg(to) + ", " + getRegCached(from) + "\n";
	}

	void op_mov(int to, int from) {
		if (to == -1) {return;}
		clearCache(to);
		cacheReg(to, getRegCached(from));
		//_asm += "mov " + getReg(to) + ", " + getReg(from) + "\n";
	}

	void op_mov_write_indirect(int to, int from) {
		if (to == -1) {return;}
		clearCache(to);
		_asm += "mov (" + getReg(to) + "), " + getReg(from) + "\n";
	}

	void op_mov_read_indirect(int to, int from) {
		if (to == -1) {return;}
		clearCache(to);
		_asm += "mov " + getReg(to) + ", (" + getReg(from) + ")\n";
	}

	void op_mov_const(int to, std::string val) {
		if (to == -1) {return;}
		clearCache(to);
		cacheReg(to, val);
		//_asm += "mov " + getReg(to) + ", " + val + "\n";
	}

	void op_add_const(int to, std::string val) {
		if (to == -1) {return;}
		clearCache(to);
		_asm += "add " + getReg(to) + ", " + val + "\n";
	}
	void op_sub_const(int to, std::string val) {
		if (to == -1) {return;}
		clearCache(to);
		_asm += "sub " + getReg(to) + ", " + val + "\n";
	}
	
	void push(int reg) {
		op_sub_const(15, "#1");
		op_mov_write_indirect(15, reg);
	}

	void pop(int reg) {
		op_mov_read_indirect(reg, 15);
		op_add_const(15, "#1");
	}
	
	void declVar(std::string name, bool moveStack=true) {
		if (moveStack) {
			op_sub_const(15, "#1");
		}
		StackVar newVar(scope, name, stackDepth);
		vars.push_back(newVar);
		stackDepth++;
	}

	void getVarAddr(std::string name, int outreg) {
		int len = vars.size();
		for (int i = len-1; i >= 0; i-- ) {
			if (vars[i].name.compare(name) == 0) {
				op_mov(outreg, 15);
				if (stackDepth-vars[i].stackDepth-1 > 0) {
					op_add_const(outreg, "#"+std::to_string(stackDepth-vars[i].stackDepth-1));
				}
				return;
			}
		}
		std::cout << "variable not in scope: '" + name + "'\n";
		exit(1);
	}

	int getFreeReg() {
		for (int i = 0; i < REG_COUNT; i++) {
			if (!usedRegs[i]) {
				return i;
			}
		}
		std::cout << "[FATAL ERROR] not enough registers.\n";
		exit(1);
		return -1;
	}
	void lockReg(int reg) {
		usedRegs[reg] = true;
	}
	void releaseReg(int reg) {
		usedRegs[reg] = false;
	}

	void beginScope() {
		scope++;
	}
	void endScope() {
		scope--;
		int len = vars.size();
		for (int i = len-1; i >= 0; i-- ) {
			if (vars[i].scope > scope) {
				vars.pop_back();
				stackDepth--;
				op_add_const(15, "#1");
			}
		}
	}
};

class ParserNode {
public:
	int id = -1;
	std::vector<ParserNode*> children = {};
	int appendChildren(const std::vector<ParserNode*>& r, std::string& str, int pos);

	void freeChildren() {
		/*for (auto c : children) {
			
			delete c;
			std::cout << "free: "<< c << ": " << typeid(*c).name() << " freed from " <<typeid(*this).name() << std::endl;
		}*/
		for (size_t i = 0; i < children.size(); i++) {
			delete children[i];
		}
		children.clear();
	}

	virtual void recursivePrint(int depth=0) {
		for (int i = 0; i < depth; i++) {
			std::cout << "  ";
		}
		std::cout << "->" << typeid(*this).name() << std::endl;
		for (auto c : children) {
			c->recursivePrint(depth+1);
		}
	}

	virtual std::vector<std::vector<ParserNode*>> grammar() {return {};};

	virtual int match(std::string& str, int pos) {
		std::vector<std::vector<ParserNode*>> rules = grammar();		
		int rules_len = rules.size();
		for (int i = 0; i < rules_len; i++) {
			int newpos = appendChildren(rules[i], str, pos);
			if (newpos >= 0) {
				// we have to iterate over all remaining rules, since we need to free the invalid ones	
				for (int j = i+1; j < rules_len; j++) {
					for (auto node : rules[j]) {
						delete node;
					}
				} 	
				return newpos;
			}
		}
		return -1;
	}
	bool addToChildren;
	ParserNode(bool _addToChildren=false) {
		addToChildren = _addToChildren;
		//std::cout <<"alloc: " << node_id << " alloc count: " << alloc_count << "\n";
		id = node_id;
		node_id++;
		alloc_count++;
	}
	virtual ~ParserNode() {
		freeChildren();
		alloc_count--;
		//std::cout <<"dealloc: " << id << " alloc count: " << alloc_count << "\n";
	}

	virtual void Eval(Compiler& compiler, int outreg) { //evaluate and put value into outreg

	}
};

int checkRule(const std::vector<ParserNode*>& nodes, std::string& str, int pos) {
	for (auto node : nodes) {
		int newpos = node->match(str, pos);
		if (newpos < 0) {
			return -1;
		} else {
			pos = newpos;
		}
	}
	return pos;
}


int ParserNode::appendChildren(const std::vector<ParserNode*>& r, std::string& str, int pos) {
	pos = checkRule(r, str, pos);
	if (pos != -1) {
		for (auto c : r) {
			if (c->addToChildren) {
				children.push_back(c);
			} else {
				delete c;
			}
		}
	} else {
		for (auto c : r) {
			delete c;
		}
		return -1;
	}
	return pos;
}

class Str : public ParserNode {
public:
	std::string text;

	Str(std::string _s, bool c = false) {
		addToChildren = c;
		text = _s;
		text += '\0';
	}

	void recursivePrint(int depth=0) {
		for (int i = 0; i < depth; i++) {
			std::cout << "  ";
		}
		std::cout << "->" << typeid(*this).name() << " '" << text << "'" << std::endl;
	}

	int match(std::string& str, int pos) {
		bool foundString = false;
		int i = 0;
		while (str[pos] != '\0' && text[i] != '\0' && str[pos] == text[i]) {
			pos++;
			i++;
			foundString = true;
		}
		if (!foundString) {
			return -1;
		}
		return pos;
	}	
	~Str() {/*std::cout<<"freed str\n";*/
	}
};

class AlphaStr : public ParserNode {
public:
	AlphaStr(bool c = false) : ParserNode(c) {}
	std::string text;

	void recursivePrint(int depth=0) {
		for (int i = 0; i < depth; i++) {
			std::cout << "  ";
		}
		std::cout << "->" << typeid(*this).name() << " '" << text << "'" << std::endl;
	}

	int match(std::string& str, int pos) {
		bool foundString = false;
		while (str[pos] != '\0' && isalnum(str[pos])) {
			text += str[pos];
			pos++;
			foundString = true;
		}
		if (!foundString) {
			return -1;
		}
		return pos;
	}	
	~AlphaStr() {}
};

class Number : public ParserNode {
public:
	Number(bool c = false) : ParserNode(c){}
	std::string text;
	int match(std::string& str, int pos) {
		bool foundString = false;
		while (str[pos] != '\0' && isdigit(str[pos])) {
			text += str[pos];
			pos++;
			foundString = true;
		}
		if (!foundString) {
			return -1;
		}
		return pos;
	}
	void recursivePrint(int depth=0) {
		for (int i = 0; i < depth; i++) {
			std::cout << "  ";
		}
		std::cout << "->" << typeid(*this).name() << " '" << text << "'" << std::endl;
	}
	~Number() {}
};

class Whitespace : public ParserNode {
public:
	int match(std::string& str, int pos) {
		while (isspace(str[pos])) {
			pos++;
		}
		return pos;
	}	
	~Whitespace() {}
};

class Token : public ParserNode {
public:
	std::string token;
	Token(std::string _s, bool c = false) {
		addToChildren = c;
		token = _s;
	}
	GRAMMAR({new Whitespace(), new Str(token, true), new Whitespace()})
	~Token() {}	
};

class Typename : public ParserNode {
public:
	Typename(bool c = false) : ParserNode(c){};
	GRAMMAR({new Token("u8", true)})
	~Typename() {}
};

template <typename T>
class Any : public ParserNode {
public:
	Any(bool c = false) : ParserNode(c){};
	int match(std::string& str, int pos) {
		int lastpos = pos;
		while (pos != -1) {
			lastpos = pos;
			pos = appendChildren({new T(true)}, str, pos);
		}
		return lastpos;
	}
	~Any() {}
};

template <typename T>
class CommaSeparated : public ParserNode {
public:
	CommaSeparated(bool c=false) : ParserNode(c){}
	int match(std::string& str, int pos) {
		int lastpos = pos;
		while (pos != -1) {
			lastpos = pos;
			pos = appendChildren({new T(true), new Whitespace(), new Str(","), new Whitespace()}, str, pos);
		}
		pos = appendChildren({new T(true)}, str, lastpos);
		if (pos != -1) {lastpos = pos;};

		return lastpos;
	}
	~CommaSeparated() {}
};

class Param : public ParserNode {
public:
	Param(bool c=false) : ParserNode(c){}
	GRAMMAR(
		{new Whitespace(), new AlphaStr(true)}
	)
	~Param() {}
};

class UnaryOP : public ParserNode {
public:
	UnaryOP(bool c = false) : ParserNode(c){}
	GRAMMAR(
		{new Token("!", true)}, 
		{new Token("-", true)}
	)
	~UnaryOP() {}
};

class MemoryOP : public ParserNode {
public:
	MemoryOP(bool c = false) : ParserNode(c){}
	GRAMMAR(
		{new Token("&", true)}, 
		{new Token("*", true)}
	)
	~MemoryOP() {}
};

class Variable : public ParserNode {
public:
	Variable(bool c=false) : ParserNode(c){}
	GRAMMAR(
		{new Whitespace(), new AlphaStr(true), new Whitespace()},
		{new MemoryOP(true), new Variable(true)}
	)
	void GetAddress(Compiler& compiler, int outreg) {
		if (children.size() == 1) {
			compiler.getVarAddr(((AlphaStr*)children[0])->text, outreg);
		} else if (children.size() == 2) {
			if (((Str*)children[0]->children[0])->text == "&") {
				std::cout << "'&' operator not allowed on left hand side\n";
				exit(1);
			} else {
				((Variable*)children[1])->GetAddress(compiler, outreg);
				compiler.op_mov_read_indirect(outreg, outreg);
			}
		}
	}
	void Eval(Compiler& compiler, int outreg) {
		if (children.size() == 1) {
			GetAddress(compiler, outreg);
			compiler.op_mov_read_indirect(outreg, outreg);
		} else if (children.size() == 2) {
			if (((Str*)children[0]->children[0])->text == "&") {
				if (typeid(*(children[1]->children[0])) != typeid(AlphaStr)) {
					std::cout << "'&' operator must be followed by variable name\n";
					exit(1);
				}
				((Variable*)children[1])->GetAddress(compiler, outreg);
			} else {
				((Variable*)children[1])->Eval(compiler, outreg);
				compiler.op_mov_read_indirect(outreg, outreg);
			}
		}
	}
	~Variable() {}
};

class Call : public ParserNode {
public:
	Call(bool c=false) : ParserNode(c){}
	use_ext_GRAMMAR
	~Call() {}
	void Eval(Compiler& compiler, int outreg) {
		if (children.size() > 0) {
			std::string name = ((AlphaStr*)children[0])->text;
			for (ParserNode* param : children[1]->children) {
				int paramreg = compiler.getFreeReg();
				compiler.lockReg(paramreg);
				param->Eval(compiler, paramreg);
				compiler.push(paramreg);
				compiler.releaseReg(paramreg);
			}
			compiler._asm += "jsr " + name + "\n";
			compiler.op_mov(outreg, 14); // return value
		}
	}
};

class Factor : public ParserNode {
public:
	Factor(bool c=false) : ParserNode(c){}
	GRAMMAR(
		{new UnaryOP(true), new Factor(true)},
		{new Number(true)},
		{new Call(true)}, // call has to be before variable to avoid mixups
		{new Variable(true)}
	)
	void Eval(Compiler& compiler, int outreg) {
		if (typeid(*children[0]) == typeid(Number)) {
			//compiler.cacheReg(outreg, "#" + ((Number*)children[0])->text);
			compiler.op_mov_const(outreg, "#" + ((Number*)children[0])->text);
		} else if (typeid(*children[0]) == typeid(Variable)) {
			((Variable*)children[0])->Eval(compiler, outreg);
		} else if (typeid(*children[0]) == typeid(Call)) {
			children[0]->Eval(compiler, outreg);
		}
	}
	~Factor() {}
};

class Term : public ParserNode {
public:
	Term(bool c=false) : ParserNode(c){}
	use_ext_GRAMMAR
	~Term() {};
	void Eval(Compiler& compiler, int outreg) {
		if (typeid(*children[0]) == typeid(Factor)) {
			children[0]->Eval(compiler, outreg);
			int tempReg = compiler.getFreeReg();
			compiler.lockReg(tempReg);

			for (ParserNode* _op : children[1]->children) {
				std::string tok = ((Str*)_op->children[0]->children[0])->text;
				_op->children[1]->Eval(compiler, tempReg);
				if (tok[0] == '*') {
					std::cout << "minirisc does not support multiplication\n";
					exit(1);
				} else if (tok[0] == '/') {
					std::cout << "minirisc does not support division\n";
					exit(1);
				}
			}

			compiler.releaseReg(tempReg);
		} else {
			children[0]->Eval(compiler, outreg);
		}
	}
};

class BinaryOP_HighPrecedence : public ParserNode {
public:
	BinaryOP_HighPrecedence(bool c = false) : ParserNode(c){}
	GRAMMAR(
		{new Token("*", true), new Term(true)},
		{new Token("/", true), new Term(true)}
	)
	~BinaryOP_HighPrecedence() {}
};

class BinaryOP_LowPrecedence : public ParserNode {
public:
	BinaryOP_LowPrecedence(bool c = false) : ParserNode(c){}
	GRAMMAR(
		{new Token("+", true), new Term(true)}, 
		{new Token("-", true), new Term(true)}
	)
	~BinaryOP_LowPrecedence() {}
};

class Expr : public ParserNode {
public:
	Expr(bool c = false) : ParserNode(c){};
	GRAMMAR(
		{new Term(true), new Any<BinaryOP_LowPrecedence>(true)}
	)
	void Eval(Compiler& compiler, int outreg) {
		if (children.size() == 0) {return;}
		children[0]->Eval(compiler, outreg);
		int tempReg = compiler.getFreeReg();
		compiler.lockReg(tempReg);

		for (ParserNode* _op : children[1]->children) {
			std::string tok = ((Str*)_op->children[0]->children[0])->text;
			_op->children[1]->Eval(compiler, tempReg);
			if (tok[0] == '+') {
				compiler.op_add(outreg, tempReg);
			} else if (tok[0] == '-') {
				compiler.op_sub(outreg, tempReg);
			}
		}

		compiler.releaseReg(tempReg);
	}
	~Expr() {};
};

set_ext_GRAMMAR(Term,
	{new Factor(true), new Any<BinaryOP_HighPrecedence>(true)},
	{new Token("("), new Expr(true), new Token(")")}
)

set_ext_GRAMMAR(Call,
	{new Whitespace(), new AlphaStr(true), new Token("("), new CommaSeparated<Expr>(true), new Token(")"), new Whitespace()}
)

class KeyWord : public ParserNode {
public:
	KeyWord(bool c = false) : ParserNode(c){};
	GRAMMAR(
		{new Str("if")}
	)

	~KeyWord() {};
};

class CodeBlock : public ParserNode {
public:
	CodeBlock(bool c = false) : ParserNode(c){}
	GRAMMAR(
		{new Token("ret", true), new Expr(true), new Token(";")},
		{new Token("var", true), new AlphaStr(true), new Token(";")},
		{new Whitespace(), new Variable(true), new Token("=", true), new Expr(true), new Token(";")},
		{new Whitespace(), new Expr(true), new Token(";")},
		{new Whitespace(), new KeyWord(true), new Token("("), new Expr(true), new Token(")"), new Token("{"), new Any<CodeBlock>(true), new Token("}")},
	)
	void Assemble(Compiler& compiler) {
		if (children.size() == 1) { // freestanding expression
			children[0]->Eval(compiler, -1);
		} else if (children.size() == 2) {
			std::string tok = ((Token*)children[0])->token; 
			if (tok == "var") {
				compiler.declVar(((AlphaStr*)children[1])->text);
			} else {
				Expr* exp = (Expr*)children[1];
				exp->Eval(compiler, 14);
				compiler.clearCache(14);
				compiler.endScope();
				compiler._asm += "rts\n";
			}
		} else if (children.size() == 3) { // assignment
			Variable* lefthand = (Variable*)children[0];
			auto righthand = children[2];

			int addr_reg = compiler.getFreeReg();
			compiler.lockReg(addr_reg);

			int val_reg = compiler.getFreeReg();
			compiler.lockReg(val_reg);

			lefthand->GetAddress(compiler, addr_reg);
			righthand->Eval(compiler, val_reg);

			compiler.op_mov_write_indirect(addr_reg, val_reg);

			compiler.releaseReg(val_reg);
			compiler.releaseReg(addr_reg);
		}
	}
	~CodeBlock() {}
};

class Function : public ParserNode {
public:
	Function(bool c=false) : ParserNode(c){}
	GRAMMAR(
		{new Whitespace(), new Token("fn", true), new Whitespace(), new AlphaStr(true), new Token("("), new CommaSeparated<Param>(true), new Token(")"), new Whitespace(), new Token("{"), new Any<CodeBlock>(true), new Token("}")}
	)
	void Assemble(Compiler& compiler) {
		if (children.size() > 0) {
			compiler._asm += ((AlphaStr*)children[1])->text + ":\n";

			compiler.beginScope();

			for (ParserNode* param : children[2]->children) {
				std::string pname = ((AlphaStr*)param->children[0])->text;
				compiler.declVar(pname, false);
			}

			for (ParserNode* block : children[3]->children) {
				((CodeBlock*)block)->Assemble(compiler);
			}
			//default return value
			/*compiler.op_mov_const(14, "#0");
			compiler.endScope();
			compiler._asm += "rts\n";*/
		}
	}
	~Function() {}
};

class Program : public ParserNode {
public:
	GRAMMAR(
		{new Any<Function>(true)}
	)
	void Assemble(Compiler& compiler) {
		if (children.size() > 0) {
			compiler._asm += "mov r15, #30\njsr start\nhalt:\njmp halt\n";
			for (ParserNode* func : children[0]->children) {
				((Function*)func)->Assemble(compiler);
			}
		}
	}
	~Program() {}
};

int main() {
	std::string code;
	for (std::string line; std::getline(std::cin, line);) {
        code += line;
    }
   	Compiler compiler;
    std::cout << code << std::endl;
	{
    	code = code + '\0';
    	Program node;
    	node.match(code, 0);
    	//node.recursivePrint();

    	node.Assemble(compiler);
    }
    std::cout << "---\n" << compiler._asm << "---\n";
    std::cout << "over: hanging allocations=" << alloc_count << "\n";
    return 0;
}