#include "ir.h"
#include "patterns.h"
#include "analysis.h"

/** Ok, so we can match on

1) instruction types

We can have different patterns, they may match a single instruction, or many. The fewer instructions it matches, the higher its priority.

2) abstract values

Each pattern can match only a single abstract value. The more specific the abstract value, the higher the priority od the pattern. 

This means that both instruction and value pattern must be able to tell the matcher what they match 





 */

using namespace analysis;


class SimplerAdd : public Visitlet {

    // this is what is actually happening
    TypeShape::Pattern * lhs = new TypeShape::Pattern(TypeShape::naked(TypeShape::Value::Type::Int));
    // if the analyses provide shortcut methods, the code can be much nicer (we can in theory get them out of the classes, but I would consider that ugly)
    TypeShape::Pattern * rhs = TypeShape::nakedInt();
    // already with sugared shortcut method
    CallTarget::Pattern * f = CallTarget::builtin("+");

    Call * e = new Call(f, { lhs, rhs });

public:

    Pattern * pattern() override {
        return e;
    }

    void match() override {
        Constants & cp(analysis<Constants>());
        if (cp[lhs].isConstant() and cp[rhs].isConstant()) {
            // missing, depends on how to instantiate Push matcher with actual operand
        } else {
            e->replaceWith(new Add(lhs, rhs));
        }
    }
};




int main(int argc, char const * argv[]) {
    Code f;
    f.append(Instruction::Push(f.pool.append(1)));
    f.append(Instruction::Push(f.pool.append(2)));
    f.append(Instruction::Add());
    f.append(Instruction::Push(f.pool.append(3)));
    f.append(Instruction::Add());

    std::cout << f;



    return EXIT_SUCCESS;
}

