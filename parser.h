#ifndef PARSER
#define PARSER

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

/*int alloc_count = 0;
int node_id = 0;*/

class ParserNode {
public:
	int id = -1;
	std::vector<ParserNode*> children = {};
	//int appendChildren(const std::vector<ParserNode*>& r, std::string& str, int pos);

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
		//id = node_id;
		//node_id++;
		//alloc_count++;
	}
	virtual ~ParserNode() {
		freeChildren();
		//alloc_count--;
		//std::cout <<"dealloc: " << id << " alloc count: " << alloc_count << "\n";
	}
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
	int appendChildren(const std::vector<ParserNode*>& r, std::string& str, int pos) {
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
};

// BASIC DEFINITIONS
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

#endif