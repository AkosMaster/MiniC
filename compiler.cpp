#include "compiler.h"
#include "language.h"

AsmState state;

void AsmState::declVar(std::string name, int forcedRegister) {
	AsmVar var;
	var.name = name;
	if (forcedRegister >= 0) {
		var.val = new ValueRef(0); // empty, custom register
		var.val->type = Register;
		var.val->reg = forcedRegister;
		var.val->writable = false; // only true when assignment happens
		state.lockReg(forcedRegister);
	} else {
		var.val = new ValueRef(); // empty, reg-allocated
		var.val->writable = false; // only true when assignment happens
	}
	var.scope = scope;
	state.comment("variable created: " + name);
	vars.push_back(var);
}

void AsmState::endBlock() {
	scope--;
	for (int i = vars.size()-1; i >= 0; i--) {
		if (vars[i].scope > scope) {
			state.comment("variable '" + vars[i].name + "' went out of scope");
			delete vars[i].val;
			vars.pop_back();
		}
	}
}

int main() {
	std::string code;
	for (std::string line; std::getline(std::cin, line);) {
        code += line;
    }
    //std::cout << code << std::endl;
	{
    	code = code + '\0';
    	Program node;
    	node.match(code, 0);
    	//node.recursivePrint();

    	node.Assemble();
    	/*ValueRef val = node.getValue();

    	ValueRef test = ValueRef();
    	val.add(test);

    	val.print();*/
    }
    std::cout << state.getAsm() << std::endl;
    //std::cout << "first free reg: " << state.getFreeReg() << std::endl;
    //std::cout << "over: hanging allocations=" << alloc_count << "\n";
    return 0;
}