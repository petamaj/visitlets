#pragma once
#include <ciso646>
#include <cassert>
#include <cstdint>

#include "patterns.h"


namespace analysis {
    class Range;
    class CallTarget;
    class TypeShape;
    class Constants;
}



/** A simple abstract value for range & constant analysis.

Works only with integers, and contains min & max values.
*/
class RangeValue {
public:
    int min() const {
        return min_;
    }

    int max() const {
        return max_;
    }

    bool isTop() const {
        return min_ == INT_MIN and max_ == INT_MAX;
    }

    bool isBottom() const {
        return min_ == INT_MAX and max_ == INT_MIN;
    }

    /** Equality, because we do not want to assume identities */
    bool operator == (RangeValue const & other) const {
        return min_ == other.min_ and max_ == other.max_;
    }

    /** Lattice ordering implementation. */
    bool operator <= (RangeValue const & other) const {
        if (isBottom() or other.isTop())
            return true;
        return min_ >= other.min_ and max_ <= other.max_;
    }

    bool mergeWith(RangeValue const & other) {
        bool result = false;
        if (other.min_ < min_) {
            result = true;
            min_ = other.min_;
        }
        if (other.max_ > max_) {
            result = true;
            max_ = other.max_;
        }
        return result;
    }

private:

    friend std::ostream & operator << (std::ostream & stream, RangeValue const & value) {
        stream << "[ " << value.min_ << " ; " << value.max_ << " ]";
        return stream;
    }

    friend class analysis::Range;

    RangeValue(int min, int max) :
        min_(min),
        max_(max) {}

    int min_;
    int max_;
};

/** A simple call target abstract value as discussed with Jan.

Target specifies the called function (actual target, not a symbol, char * is chosen here only for printing purposes of the mockup example). The call target can either be certain, in which case we are statically sure it will call the required function, or possible, which can have many meanings, but in the simplest form it is a hint to the optimizer that a fastpath to that target might be useful.
*/
class CallTargetValue {
public:
    enum class Accuracy {
        Possible,
        Certain,
    };

    Accuracy accuracy() const {
        return accuracy_;
    }

    char const * target() const {
        return target_;
    }

    bool isTop() const {
        return target_ == nullptr and accuracy_ == Accuracy::Possible;
    }

    bool isBottom() const {
        return target_ == nullptr and accuracy_ == Accuracy::Certain;
    }

    bool operator == (CallTargetValue const & other) const {
        return target_ == other.target_ and accuracy_ == other.accuracy_;
    }

    bool operator <= (CallTargetValue const & other) const {
        if (isBottom() or other.isTop())
            return true;
        if (target_ != other.target_)
            return false;
        return (accuracy_ == Accuracy::Possible or other.accuracy_ == Accuracy::Certain);
    }

    bool mergeWith(CallTargetValue const & other) {
        bool result = false;
        if (target_ != other.target_) {
            result = true;
            target_ = nullptr;
        }
        if (accuracy_ == Accuracy::Certain and other.accuracy_ == Accuracy::Possible) {
            result = true;
            accuracy_ = Accuracy::Possible;
        }
        return result;
    }

private:
    friend std::ostream & operator << (std::ostream & stream, CallTargetValue const & value) {
        if (value.target_ == nullptr)
            stream << "top or bottom";
        else
            if (value.accuracy_ == CallTargetValue::Accuracy::Possible)
                stream << "possible " << value.target_;
            else
                stream << value.target_;
        return stream;
    }

    friend class analysis::CallTarget;

    CallTargetValue(Accuracy accuracy, char const * target) :
        accuracy_(accuracy),
        target_(target) {}

    Accuracy accuracy_;

    char const * target_;
};

/** Mockup type and shape value to accomodate the naked int in Jan's example.
*/
class TypeShapeValue {
public:
    enum class Type {
        Bottom,
        Int,
        Double,
        Any,
    };

    enum class Shape {
        Naked,
        Any,
    };

    Type type() const {
        return type_;
    }

    Shape shape() const {
        return shape_;
    }

    bool isBottom() const {
        return type_ == Type::Bottom;
    }

    bool isTop() const {
        return type_ == Type::Any and shape_ == Shape::Any;
    }

    bool operator == (TypeShapeValue const & other) const {
        return type_ == other.type_ and shape_ == other.shape_;
    }

    bool operator <= (TypeShapeValue const & other) const {
        if (isBottom() or other.isTop())
            return true;
        // TODO fill this in based on the lattice
        return false;
    }

    bool mergeWith(TypeShapeValue const & other) {
        return false;
    }

private:
    friend std::ostream & operator << (std::ostream & stream, TypeShapeValue const & value) {
        if (value.shape_ == TypeShapeValue::Shape::Naked)
            stream << "naked ";
        switch (value.type_) {
            case TypeShapeValue::Type::Bottom:
                stream << "bottom";
                break;
            case TypeShapeValue::Type::Any:
                stream << "any";
                break;
            case TypeShapeValue::Type::Int:
                stream << "Int";
                break;
            case TypeShapeValue::Type::Double:
                stream << "Double";
                break;
        }
        return stream;
    }

    friend class analysis::TypeShape;

    TypeShapeValue(Type type, Shape shape) :
        type_(type),
        shape_(shape) {}

    Type type_;
    Shape shape_;
};

/** Simply tells us whether it is a constant, or not. The int * is stupid, but assumes that we will have a pointer to the constant SEXP. It can of course be implemented in myriads of ther ways and is not important now.
 */
class ConstantsValue {
public:
    int * value() const {
        return value_;
    }

    bool isConstant() const {
        return (uintptr_t) value_ > 1;
    }

    bool isTop() const {
        return value_ == nullptr;
    }

    bool isBottom() const {
        return value_ == (int*)1;
    }

    bool operator == (ConstantsValue const & other) const {
        return value_ == other.value_;
    }

    bool operator <= (ConstantsValue const & other) const {
        if (isBottom() or other.isTop())
            return true;
        return value_ == other.value_;
    }

    bool mergeWith(ConstantsValue const & other) {
        return false;
    }

private:

    friend std::ostream & operator << (std::ostream & stream, ConstantsValue const & value) {
        if (value.value_ != nullptr)
            stream << "constant";
        else
            stream << "top or bottom";
        return stream;
    }

    friend class analysis::Constants;

    ConstantsValue(int * value):
        value_(value) {
    }

    int * value_;
};


/** Mockup analyses, I am only concerned with their vaye types */

namespace analysis {

    class Analysis {

    };

    class Range : public Analysis {
    public:
        typedef RangeValue Value;

        typedef TypePattern<Range> Pattern;

        static Value constant(int value) {
            return Value(value, value);
        }

        static Value range(int min, int max) {
            assert(min <= max and "min must be smaller or equal to max");
            return Value(min, max);
        }

        static Value top() {
            return Value(INT_MIN, INT_MAX);
        }

        static Value bottom() {
            return Value(INT_MAX, INT_MIN);
        }

        Value const & operator [] (::Pattern * p) {
            static Value v(top());
            return v;
        }

    };

    class CallTarget : public Analysis {
    public:
        typedef CallTargetValue Value;

        typedef TypePattern<CallTarget> Pattern;

        static Pattern * builtin(char const * target) {
            return new Pattern(certain(target));
        }

        static Value certain(char const * target) {
            return Value(Value::Accuracy::Certain, target);
        }

        static Value possible(char const * target) {
            return Value(Value::Accuracy::Possible, target);
        }

        static Value top() {
            return Value(Value::Accuracy::Possible, nullptr);
        }

        static Value bottom() {
            return Value(Value::Accuracy::Certain, nullptr);
        }

        Value const & operator [] (::Pattern * p) {
            static Value v(top());
            return v;
        }

    };

    class TypeShape : public Analysis {
    public:
        typedef TypeShapeValue Value;

        typedef TypePattern<TypeShape> Pattern;

        // creating the patterns can be simplified like this
        static Pattern * nakedInt() {
            return new Pattern(naked(Value::Type::Int));
        }


        static Value create(Value::Type type, Value::Shape shape = Value::Shape::Any) {
            return Value(type, shape);
        }

        static Value naked(Value::Type type) {
            return Value(type, Value::Shape::Naked);
        }

        static Value top() {
            return Value(Value::Type::Any, Value::Shape::Any);
        }

        static Value bottom() {
            return Value(Value::Type::Bottom, Value::Shape::Any);
        }

        Value const & operator [] (::Pattern * p) {
            static Value v(top());
            return v;
        }

    };

    class Constants : public Analysis {
    public:
        typedef ConstantsValue Value;

        typedef TypePattern<Constants> Pattern;

        static Value constant(int * what) {
            return Value(what);
        }

        static Value top() {
            return Value(nullptr);
        }

        static Value bottom() {
            return Value((int*)1);
        }

        Value const & operator [] (::Pattern * p) const {
            static Value v(top());
            return v;
        }
    };

}
