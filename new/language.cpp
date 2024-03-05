#include "language.h"

set_ext_GRAMMAR(Term,
	{new Call(true)},
	{new Number(true)},
	{new Token("("), new Expr(true), new Token(")")},
	{new VarLabel(true)}
)

set_ext_GRAMMAR(If,
	{new Token("if"), new Token("("), new Expr(true), new Token(")"), new CodeBlock(true)}
)

set_ext_GRAMMAR(While,
	{new Token("while"), new Token("("), new Expr(true), new Token(")"), new CodeBlock(true)}
)

set_ext_GRAMMAR(Call,
	{new Whitespace(), new AlphaStr(true), new Token("("), new CommaSeparated<Expr>(true), new Token(")")}
)

set_ext_GRAMMAR(Statement,
	{new Declaration(true), new Token(";")},
	{new Assignment(true), new Token(";")},
	{new If(true)},
	{new While(true)},
	{new Call(true), new Token(";")},
	{new Return(true), new Token(";")},
	{new CodeBlock(true)}
)

ValueRef* Call::getValue(bool dropRet=false) {
	std::string fname = ((AlphaStr*)children[0])->text;

	state.pushUsedRegs();

	int preg = 13;
	for (ParserNode* param : children[1]->children) {
		ValueRef* pval = ((Expr*)param)->getValue();
		
		if (pval->type == Constant) {
			state.op_mov_const(preg, pval->constval);
		} else if (pval->type == Register) {
			state.op_mov(preg, pval->reg);
		}
		state.lockReg(preg);
		preg--;
		delete pval;
	}

	state._asm += "jsr " + fname + "\n";

	preg = 13;
	for (ParserNode* param : children[1]->children) {
		state.freeReg(preg);
		preg--;
	}

	state.popUsedRegs();
	if (dropRet) {
		return nullptr;
	}
	ValueRef* retval = new ValueRef();
	state.op_mov(retval->reg, 14);
	return retval;
}

ValueRef* Term::getValue() {
	if (typeid(*children[0]) == typeid(Number)) {
		// return constant vref
		return new ValueRef( stoi(((Number*)children[0])->text) );
	} else if (typeid(*children[0]) == typeid(Expr)) {
		return ((Expr*)children[0])->getValue();
	} else if (typeid(*children[0]) == typeid(VarLabel)) {
		return ((VarLabel*)children[0])->getValue();
	} else if (typeid(*children[0]) == typeid(Call)) {
		return ((Call*)children[0])->getValue();
	}
	std::cout << "Term getValue failed\n";
	exit(1);
}

ValueRef* Expr::getValue() {

	if (children[1]->children.size() == 0) {
		return ((Term*)children[0])->getValue();
	}

	ValueRef* sum = new ValueRef(0);
	ValueRef* first = ((Term*)children[0])->getValue();
	sum->set(first);
	delete first;

	for (ParserNode* o : children[1]->children) {
		BinaryOP* op = (BinaryOP*)o;
		ValueRef* val = op->RightHandSide()->getValue();

		std::string operation = op->Operation();
		if (operation == "+") {
			sum->add(val);
		} else if (operation == "-") {
			sum->add(val, -1);
		} else {
			std::cout << "operation '" + operation + "' not implemented\n";
			exit(1);
		}
		delete val;
	}
	return sum;
}

void Declaration::Assemble() {
	state.declVar(((AlphaStr*)children[0])->text);
}

void VarLabel::setValue(ValueRef* val) {
	if (typeid(*children[0]) == typeid(AlphaStr)) { // simple variable assignment
		std::string name = ((AlphaStr*)children[0])->text;
		ValueRef* var = state.getVar(name);
		var->set(val);
	} else { //deref
		VarLabel* label = ((VarLabel*)children[0]);
		ValueRef* addr = label->getValue();
		if (addr->type != Register) {
			std::cout << "internal error setValue\n";
			exit(1);
		}
		if (val->type == Constant) {
			state.op_mov_const(14, val->constval); // can only indirect write from register....
			state.op_mov_write_deref(addr->reg, 14);
		} else {
			state.op_mov_write_deref(addr->reg, val->reg);
		}
	}
}

ValueRef* VarLabel::getValue() {
	if (typeid(*children[0]) == typeid(AlphaStr)) { // simple variable get
		std::string name = ((AlphaStr*)children[0])->text;
		
		return state.getVar(name)->getCopy(); // need to return new valueref, so that value reference is not deleted later
	} else { //deref
		VarLabel* label = ((VarLabel*)children[0]);
		ValueRef* val = label->getValue();
		val->deref();
		return val;
	}
}

void Assignment::Assemble() {
	ValueRef* rightHand = ((Expr*)children[1])->getValue();
	((VarLabel*)children[0])->setValue(rightHand);
	delete rightHand;
}

void If::Assemble() {
	ValueRef* condition = ((Expr*)children[0])->getValue();
	CodeBlock* onTrue = (CodeBlock*)children[1];

	if (condition->type == Constant) {

		if (condition->constval == 0) {
			return;
		} else {
			onTrue->Assemble();
		}

	} else if (condition->type == Register) {
		state._asm += "cmp r" + std::to_string(condition->reg) + ", #0\n";

		static int elseID = 0;
		elseID++;
		state._asm += "jz else_" + std::to_string(elseID) + "\n";
		onTrue->Assemble();
		state.symbol("else_" + std::to_string(elseID));
	}
}

void While::Assemble() {
	ValueRef* condition = ((Expr*)children[0])->getValue();
	CodeBlock* onTrue = (CodeBlock*)children[1];

	if (condition->type == Constant) {

		if (condition->constval == 0) {
			return;
		} else {
			onTrue->Assemble();
		}

	} else if (condition->type == Register) {
		static int whileID = 0;
		whileID++;
		
		state.symbol("wbegin_" + std::to_string(whileID));
		
		state._asm += "cmp r" + std::to_string(condition->reg) + ", #0\n";
		state._asm += "jz wend_" + std::to_string(whileID) + "\n";
		onTrue->Assemble();
		
		state._asm += "jmp wbegin_" + std::to_string(whileID) + "\n";
		state.symbol("wend_" + std::to_string(whileID));
	}
}

void Return::Assemble() {
	ValueRef* retval = ((Expr*)children[0])->getValue();
	if (retval->type == Constant) {
		state.op_mov_const(14, retval->constval);
	} else if(retval->type == Register) {
		state.op_mov(14, retval->reg);
	}
	state._asm += "rts\n";
	delete retval;
}

void Statement::Assemble() {
	if (children.size() == 0) {return;}
	if (typeid(*children[0]) == typeid(Declaration)) {
		((Declaration*)children[0])->Assemble();
	} else if (typeid(*children[0]) == typeid(Assignment)) {
		((Assignment*)children[0])->Assemble();
	} else if (typeid(*children[0]) == typeid(If)) {
		((If*)children[0])->Assemble();
	} else if (typeid(*children[0]) == typeid(While)) {
		((While*)children[0])->Assemble();
	} else if (typeid(*children[0]) == typeid(Call)) {
		((Call*)children[0])->getValue(true);
	} else if (typeid(*children[0]) == typeid(CodeBlock)) {
		((CodeBlock*)children[0])->Assemble();
	} else if (typeid(*children[0]) == typeid(Return)) {
		((Return*)children[0])->Assemble();
	}
}

void CodeBlock::Assemble() {
	if (children.size() == 0) {return;}
	state.beginBlock();
	for (ParserNode* statement : getStatements()) {
		((Statement*)statement)->Assemble();
	}
	state.endBlock();
}

void Function::Assemble() {
	if (children.size() == 0) {return;}
	state.symbol(getName());
	state.beginBlock();

	int preg = 13;
	for (ParserNode* param : children[1]->children) {
		std::string pname = ((AlphaStr*)param)->text;
		state.comment("param '" + pname + "' defined as local var");
		state.declVar(pname, preg);
		preg--;
	}

	((CodeBlock*)children[2])->Assemble();

	state.endBlock();
}

void Program::Assemble() {
	state._asm += "mov r15, #30\njsr start\nhalt:\njmp halt\n";
	if (children.size() == 0) {return;}
	for (ParserNode* func : children[0]->children) {
		((Function*)func)->Assemble();
	}
	state._asm += "rts\n";
}















