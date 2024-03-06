#ifndef COMPILER
#define COMPILER
#include <vector>
#include <iostream>
#include <typeinfo>
#include <ctype.h>
#include <string>

#define NO_COMMENTS

class ValueRef;
class AsmVar {
public:
	std::string name;
	//ValueRef* val;
	int reg=-1;
	int scope = 999;
};

class AsmState {
	int stackDepth = 0;
	std::vector<AsmVar> vars;
	bool reachable = true;
	int usedRegisters[14] = {0};
public:

	std::string _asm;
	int getFreeReg(int preferred = -1) {
		if (preferred >= 0) {
			if (usedRegisters[preferred] == 0) {
				return preferred;
			}
		}
		for (int i = 0; i < 14; i++) {
			if (usedRegisters[i] == 0) {
				return i;
			}
		}
		std::cout << "unable to allocate register\n";
		exit(1);
	}
	void lockReg(int reg) {
		usedRegisters[reg]++;
	}
	void freeReg(int reg) {
		if (usedRegisters[reg] > 0) {
			usedRegisters[reg]--;
		} else {
			std::cout << "attempt to free non-allocated register\n";
			exit(1);
		}
	}

	void op_mov(int to, int from) {
		if (to == from) {return;}
		if (!reachable) {return;}
		_asm += "mov r" + std::to_string(to) + ", r" + std::to_string(from) + "\n";
	}

	void op_add(int to, int from) {
		if (!reachable) {return;}
		_asm += "add r" + std::to_string(to) + ", r" + std::to_string(from) + "\n";
	}

	void op_sub(int to, int from) {
		if (!reachable) {return;}
		_asm += "sub r" + std::to_string(to) + ", r" + std::to_string(from) + "\n";
	}

	void op_mov_const(int to, int val) {
		if (!reachable) {return;}
		_asm += "mov r" + std::to_string(to) + ", #" + std::to_string(val) + "\n";
	}

	void op_mov_label(int to, std::string label) {
		if (!reachable) {return;}
		_asm += "mov r" + std::to_string(to) + ", #" + label + "\n";
	}

	void op_mov_read_deref(int to, int from) {
		if (!reachable) {return;}
		_asm += "mov r" + std::to_string(to) + ", (r" + std::to_string(from) + ")\n";
	}

	void op_mov_write_deref(int to, int from) {
		if (!reachable) {return;}
		_asm += "mov (r" + std::to_string(to) + "), r" + std::to_string(from) + "\n";
	}

	void op_add_const(int to, int val) {
		if (!reachable) {return;}
		if (val == 0) {return;}
		_asm += "add r" + std::to_string(to) + ", #" + std::to_string(val) + "\n";
	}

	void op_sub_const(int to, int val) {
		if (!reachable) {return;}
		if (val == 0) {return;}
		_asm += "sub r" + std::to_string(to) + ", #" + std::to_string(val) + "\n";
	}

	void declVar(std::string name, int forcedRegister=-1);
	int getVar(std::string name) {
		for (int i = vars.size()-1; i >= 0; i--) {
			if (vars[i].name == name) {
				return vars[i].reg;
			}
		}
		std::cout << "variable '"+name+"' not in scope\n";
		exit(1);
	}

	void comment(std::string c) {
		#ifndef NO_COMMENTS
		_asm += "; " + c + "\n";
		#endif
 	}
 	void symbol(std::string s) {
		_asm += s + ":\n";
		reachable = true;
 	}

	std::string getAsm() {
		return _asm;
	}

	void op_ret() {
		if (!reachable) {return;}
		_asm += "rts\n";
		reachable = false;
	}

	void pushUsedRegs() {
		for (int i = 0; i < 14; i++) {
			if (usedRegisters[i] > 0) {
				_asm += "sub r15, #1\n";
				_asm += "mov (r15), r" + std::to_string(i) + "\n";
			}
		}
	}

	void popUsedRegs() {
		for (int i = 13; i >= 0; i--) {
			if (usedRegisters[i] > 0) {
				_asm += "mov r" + std::to_string(i) + ", (r15)\n";
				_asm += "add r15, #1\n";
			}
		}	
	}

	int scope = 0;
	void beginBlock() {
		scope++;
	}
	void endBlock();
};

class AsmState;
extern AsmState state;

enum StorageType {
	Constant,
	Register
};

#define OPT_FLOW

class ValueRef {
public:
	StorageType type;
	int reg;
	int constval;
	bool writable = true;

	ValueRef() : type(Register) {
		type = Register;
		reg = state.getFreeReg();
		state.lockReg(reg);
	}
	ValueRef(int val) : type(Constant) {
		type = Constant;
		constval = val;
	}
	ValueRef(ValueRef& cpy) {
		std::cout << "valueref should never be copied\n";
		exit(1);
	}

	bool copy = false;
	/*ValueRef* shallowCopy() {
		if (type == Constant) {
			return new ValueRef(constval);
		} else {
			ValueRef* cpy = new ValueRef(0);
			cpy->reg = reg;
			cpy->copy = true;
			cpy->writable = writable;
			cpy->type = Register;
		}
	}*/

	void deref() {
		if (type == Register) {
			if (!writable) {
				writable = true;
				int oldreg = reg;
				state.freeReg(reg);
				reg = state.getFreeReg();
				state.lockReg(reg);
				state.op_mov_read_deref(reg, oldreg);
			} else {
				state.op_mov_read_deref(reg, reg);
			}
		} else {
			std::cout << "not implemented\n";
			exit(1);
		}
	}

	void set(ValueRef* rhs) {
		if (!writable) {
			std::cout << "attempt to modify non-writable value\n";
			exit(1);
		}
		if (type == Constant) {
			if (rhs->type == Constant) {
				constval = rhs->constval;
				return;
			} else if (rhs->type == Register) {
				type = Register;
				reg = state.getFreeReg();
				state.lockReg(reg);
				state.op_mov(reg, rhs->reg);
				return;
			}
		} else if (type == Register) {
			if (rhs->type == Constant) {
				state.op_mov_const(reg, rhs->constval);
				return;
			} else if (rhs->type == Register) {
				state.op_mov(reg, rhs->reg);
				return;
			}
		}
	}

	void add(ValueRef* rhs, int mul=1) {
		if (!writable) {
			std::cout << "attempt to add to non-writable value\n";
			exit(1);
		}
		if (type == Constant) {
			if (rhs->type == Constant) {
				constval += rhs->constval * mul;
				return;
			} else if (rhs->type == Register) {
				type = Register;
				reg = state.getFreeReg();
				state.lockReg(reg);
				state.op_mov_const(reg, constval);
				if (mul > 0) {
					state.op_add(reg, rhs->reg);
				} else {
					state.op_sub(reg, rhs->reg);
				}
				return;
			}
		} else if (type==Register) {
			if (rhs->type == Constant) {
				if (mul > 0) {
					state.op_add_const(reg, rhs->constval);
				} else {
					state.op_sub_const(reg, rhs->constval);
				}
				return;
			} else if (rhs->type == Register) {
				if (mul > 0) {
					state.op_add(reg, rhs->reg);
				} else {
					state.op_sub(reg, rhs->reg);
				}
				return;
			}
		}
	}

	/*void shift(ValueRef* rhs) {
		if (!writable) {
			std::cout << "attempt to shift non-writable value\n";
			exit(1);
		}
		if (type == Constant) {
			if (rhs->type == Constant) {
				constval = constval << rhs->constval;
				return;
			} else if (rhs->type == Register) {
				type = Register;
				reg = state.getFreeReg();
				state.lockReg(reg);
				state.op_mov_const(reg, constval);
				
				state.op_shift(reg, rhs->reg);
				
				return;
			}
		} else if (type==Register) {
			if (rhs->type == Constant) {
				
				state.op_shift_const(reg, rhs->constval);
				
				return;
			} else if (rhs->type == Register) {
				
				state.op_shift(reg, rhs->reg);
				
				return;
			}
		}
	}*/

	~ValueRef() {
		if (type == Register && !copy) {
			state.freeReg(reg);
		}
	}
};

#endif



















