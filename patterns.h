#pragma once

#include <ciso646>
#include <vector>

#include "ir.h"

class Matcher;

class Pattern {
public:
    virtual ~Pattern() {}

    virtual void registerWith(Matcher * matcher) = 0;

    /** Pattern replacement. 
     */
    void replaceWith(Pattern * with) {

    }

protected:
    /** Each pattern has the location to which it matches. This is always an instruction.

    For instruction patterns, this is the location of the instruction matched to the pattern.

    For value patterns, this is the instruction that produced the value, or in more general terms, a handle that we can use to query other analyses to give us additional details.

    TODO this is a big vague still as it would work lovely for SSA but have problems elsewhere...
    */
    Instruction * location_;

    Code * code_;

};

namespace analysis {
    class Analysis;
}

class Visitlet {
public:
    virtual Pattern * pattern() = 0;

    virtual void match() = 0;

    /** Destructor gets rid of the pattern the visitlet matches accordingly. */
    virtual ~Visitlet() {
        delete pattern();
    }

protected:

    // TODO This should be in the matcher actually and the analysis should only retrieve it from it
    template<typename ANALYSIS>
    ANALYSIS & analysis() {
        static ANALYSIS a;
        return a;
    }
};

/** Matcher is responsible for creating the matching automaton to search for the visitlet patterns.

I am not dealing with the actual creation, but it should not be too hard - in fact guys at CTU are doing research into tree and graph searching algorithms so they can point me to highly optimized variants, I would hope.

All I need is list of visitlets, and then for each instruction, knowing which visitlets will match it, and for each analysis, which visitlets will match on its abstract value (this is to allow prioritizing).
*/
class Matcher {
public:


    template<typename ANALYSIS>
    void attachToAnalysis(Pattern * p, typename ANALYSIS::Value const & value) {
        std::cout << "Attaching pattern " << p << " to analysis " << analysisIndex<ANALYSIS>() << " and value " << value << std::endl;
    }

    void attachToInstruction(Pattern * p, Instruction::Opcode opcode) {
        std::cout << "Attaching pattern " << p << " to instruction " << Instruction::opcodeToStr(opcode) << std::endl;
    }

    void addVisitlet(Visitlet * visitlet) {
        visitlets_.push_back(visitlet);
        visitlet->pattern()->registerWith(this);
    }

private:

    size_t reserveAnalysis() {
        size_t result = analyses_.size();
        analyses_.push_back(nullptr);
        return result;
    }

    template<typename ANALYSIS>
    size_t analysisIndex() {
        static size_t index = reserveAnalysis();
        return index;
    }

    std::vector<Visitlet *> visitlets_;
    std::vector<analysis::Analysis *> analyses_;
};




/** Do not care pattern. */
class Any : public Pattern {};


/** Template for simple creation of abstract value patterns. 
 */
template<typename ANALYSIS>
class TypePattern : public Pattern {
public:

    TypePattern(typename ANALYSIS::Value const & value) :
        value_(value) {}


    typename ANALYSIS::Value const & value() const {
        return value_;
    }

    void registerWith(Matcher * matcher) {
        matcher->attachToAnalysis<ANALYSIS>(this, value_);
    }

private:
    typename ANALYSIS::Value value_;
};

/** Base class for instruction patterns.
*/
class InstructionPattern : public Pattern {
public:

    /** Returns list of opcodes the pattern matches. */
    virtual std::vector<Instruction::Opcode> const & matches() const = 0;

    /** Returns the number of operands of *all* instructions the pattern matches.
    */
    virtual size_t operands() const = 0;

    /** Returns the pattern to be matched for index-th operand of the instruction.
    */
    virtual Pattern * operand(size_t index) const = 0;

    // --- proxy for the slim instruction ---

    /** Returns the PC of the matched instruction. */
    size_t pc() const {
        return reinterpret_cast<char *>(location_) - code_->at<char>(0);
    }

    /** Returns the opcode of the matched instruction. */
    Instruction::Opcode opcode() const {
        return location_->opcode;
    }

    /** Returs the size of the matched instruction. */
    size_t size() const {
        return location_->size();
    }

    void registerWith(Matcher * matcher) override {
        for (Instruction::Opcode opcode : matches())
            matcher->attachToInstruction(this, opcode);
        for (size_t i = 0, e = operands(); i, e; ++i)
            operand(i)->registerWith(matcher);
    }
};




class Push : public InstructionPattern {
public:

    std::vector<Instruction::Opcode> const & matches() const override {
        static std::vector<Instruction::Opcode> m({ Instruction::Opcode::Push });
        return m;
    }

    size_t operands() const override {
        return 0;
    }

    Pattern * operand(size_t index) const override {
        assert(false and "instruction has no operands");
        return nullptr;
    }

    /** Using the knowledge of the matched location and code, returns not the index to the constant pool, but the actual value it pushes on stack. */
    int immediate() const {
        return code_->pool[reinterpret_cast<Instruction::Push*>(location_)->index];
    }
};


/** This is a non-templated version of the Add instruction.

    If we made it templated, then its lhs and rhs would point to proper types they have based on the pattern. Having Add, or anything with static number of pops templated will be really easy. 
 */
class Add : public InstructionPattern {
public:
    Add(Pattern * lhs, Pattern * rhs) :
        lhs_(lhs),
        rhs_(rhs) {}

    ~Add() {
        delete lhs_;
        delete rhs_;
    }

    std::vector<Instruction::Opcode> const & matches() const override {
        static std::vector<Instruction::Opcode> m({ Instruction::Opcode::Add });
        return m;
    }

    size_t operands() const override {
        return 2;
    }

    Pattern * operand(size_t index) const override {
        if (index == 0)
            return lhs_;
        else if (index == 1)
            return rhs_;
        else
            assert(false and "instruction has only two operands");
        return nullptr;
    }

    Pattern * lhs() const { // everything is pointers for simplicity and similarity with C where references are, sadly, not
        return lhs_;
    }

    Pattern * rhs() const {
        return rhs_;
    }

private:

    Pattern * lhs_;
    Pattern * rhs_;
};

/** Pattern for matching call instruction. 

    Call instruction has f, the function it calls to, and a variable number of operands. This is the non-templated version. The templated version can either use std::tuple, with not really nice syntax, or with some more thinking we can create our better syntax - but the syntax of variable sized instruction patterns would never be as straightforward as say for Add. 
 */
class Call : public InstructionPattern {
public:

    Call(Pattern * f, std::initializer_list<Pattern*> arguments) {
        operands_.push_back(f);
        for (Pattern * p : arguments)
            operands_.push_back(p);
    }
        
    ~Call() {
        for (Pattern * p : operands_)
            delete p;
    }

    std::vector<Instruction::Opcode> const & matches() const override {
        static std::vector<Instruction::Opcode> m({ Instruction::Opcode::Call });
        return m;
    }

    size_t operands() const override {
        return operands_.size();
    }

    Pattern * operand(size_t index) const override {
        return operands_[index];
    }

    Pattern * f() const {
        return operands_[0];
    }

    size_t arguments() const {
        return operands_.size() - 1;
    }

    Pattern * argument(size_t index) const {
        return operands_[index + 1];
    }

private:

    std::vector<Pattern *> operands_;
};
