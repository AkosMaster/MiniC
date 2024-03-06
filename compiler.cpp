#include "compiler.h"
#include "language.h"

AsmState state;

void AsmState::declVar(std::string name, int forcedRegister) {
	AsmVar var;
	var.name = name;
	if (forcedRegister >= 0) {
		var.reg = forcedRegister;
	} else {
		var.reg = state.getFreeReg();
	}
	state.lockReg(var.reg);
	var.scope = scope;
	state.comment("variable created: " + name);
	vars.push_back(var);
}

void AsmState::endBlock() {
	scope--;
	for (int i = vars.size()-1; i >= 0; i--) {
		if (vars[i].scope > scope) {
			state.comment("variable '" + vars[i].name + "' went out of scope");
			//delete vars[i].val;
			state.freeReg(vars[i].reg);
			vars.pop_back();
		}
	}
}

int main() {
	std::string data;
	std::string code;
	bool in_code = false;
	for (std::string line; std::getline(std::cin, line);) {
        if (!in_code) {
        	data += line + "\n";
        	if (line == "CODE") {in_code = true;}
    	} else {
    		code += line + "\n";
    	}
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
    std::cout << data << state.getAsm() << std::endl;
    //std::cout << "first free reg: " << state.getFreeReg() << std::endl;
    //std::cout << "over: hanging allocations=" << alloc_count << "\n";
    return 0;
}