#include "Halide.h"

// By convention, Generators always go in a .cpp file, usually with no
// corresponding .h file. They can be enclosed in any C++ namespaces
// you like, but the anonymous namespace is often the best choice.
//
// It's normally considered Best Practice to have exactly one Generator
// per .cpp file, and to have the .cpp file name match the generator name
// with a "_generator" suffix (e.g., Generator with name "foo" should
// live in "foo_generator.cpp"), as it tends to simplify build rules,
// but neither of these are required.

namespace {

enum class SomeEnum { Foo, Bar };

// Note the inheritance using the Curiously Recurring Template Pattern
class Example : public Halide::Generator<Example> {
public:
    // GeneratorParams, ImageParams, and Params are (by convention)
    // always public and always declared at the top of the Generator,
    // in the order
    //    GeneratorParam(s)
    //    ImageParam(s)
    //    Param(s)
    //
    // Note that the ImageParams/Params will appear in the C function
    // call in the order they are declared. (GeneratorParams are always
    // referenced by name, not position, so their order is irrelevant.)
    //
    // All Param variants declared as Generator members must have explicit
    // names, and all such names must match the regex [A-Za-z_][A-Za-z_0-9]*
    // (i.e., essentially a C/C++ variable name). (Note that autogenerated
    // Param names do not match this pattern, and thus will be rejected.)
    // By convention, the name should match the member-variable name.

    // GeneratorParams can be float or ints: {default} or {default, min, max}
    // (Note that if you want to specify min and max, you must specify both.)
    GeneratorParam<float> compiletime_factor{ "compiletime_factor", 1, 0, 100 };
    GeneratorParam<int> channels{ "channels", 4 };
    // ...or enums: {default, name->value map}
    GeneratorParam<SomeEnum> enummy{ "enummy",
                                     SomeEnum::Foo,
                                     { { "foo", SomeEnum::Foo },
                                       { "bar", SomeEnum::Bar } } };
    // ...or bools: {default}
    GeneratorParam<bool> flag{ "flag", true };

    // Halide::Type is supported as though it was an enum.
    // It's most useful for customizing the type of input or output image params.
    GeneratorParam<Halide::Type> output_type{ "output_type", UInt(8) };

    // These are bad names that will produce errors at build time:
    // GeneratorParam<bool> badname{ " flag", true };
    // GeneratorParam<bool> badname{ "flag ", true };
    // GeneratorParam<bool> badname{ "0flag ", true };
    // GeneratorParam<bool> badname{ "", true };
    // GeneratorParam<bool> badname{ "\001", true };
    // GeneratorParam<bool> badname{ "a name? with! stuff*", true };

    // Note that a leading underscore is legal-but-reserved in C,
    // but it's outright forbidden here. (underscore after first char is ok.)
    // GeneratorParam<bool> badname{ "_flag", true };

    // We also forbid two underscores in a row.
    // GeneratorParam<bool> badname{ "f__lag", true };

    // Param (and ImageParam) are arguments passed to the filter when
    // it is executed (as opposed to the Generator, during compilation).
    // When jitting, there is effectively little difference between the
    // two (at least for scalar values). Note that we set a default value of
    // 1.0 so that invocations that don't set it explicitly use a predictable value.
    Param<float> runtime_factor{ "runtime_factor", 1.0 };

    // The help() method of a generator should print out a description
    // of what the Generator is. This is triggered by the -help option
    // when compiling this file along with GenGen.cpp
    void help(std::ostream &out) override {
        out << "This is an example generator!\n";
    }

    // The build() method of a generator defines the actual pipeline
    // and returns the output Func.
    Func build() override {
        Func f("f"), g("g");
        Var x, y, c;

        f(x, y) = max(x, y);
        g(x, y, c) = cast(output_type, f(x, y) * c * compiletime_factor * runtime_factor);

        g.bound(c, 0, channels).reorder(c, x, y).unroll(c);

        // Note that we can use the Generator method natural_vector_size()
        // here; this produces the width of the SIMD vector being targeted
        // divided by the width of the data type.
        // g.vectorize(x, natural_vector_size(output_type));

        return g;
    }

    // Your should put correctness and performance tests for a
    // generator into the test() method. This is triggered by the
    // -test option when compiling this file along with GenGen.cpp
    bool test() override {
        // Generator params must be set before calling build.
        compiletime_factor.set(2.5f);
        output_type.set(Int(32));

        // Build the pipeline.
        Func g = build();

        // Set the runtime params. These can be set before or after
        // calling build.
        runtime_factor.set(2.0f);

        // Run it.
        Image<int> out = g.realize(10, 10, 3);

        // Check the output is as expected:
        for (int c = 0; c < out.channels(); c++) {
            for (int y = 0; y < out.height(); y++) {
                for (int x = 0; x < out.width(); x++) {
                    int correct = std::max(x, y) * c * 5;
                    if (correct != out(x, y, c)) {
                        printf("out(%d, %d, %d) = %d instead of %d\n",
                               x, y, c, out(x, y, c), correct);
                        return false;
                    }
                }
            }
        }

        return true;
    }
};

// If you're only using a Generator with the JIT, you don't need to register it;
// however, registering it is needed for working seamlessly with the ahead-of-time
// compilation tools, so it's generally recommended to always register it.
// (As with Params, the name is constrained to C-like patterns.)
Halide::RegisterGenerator<Example> register_example{"example"};

}  // namespace
