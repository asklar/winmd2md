#pragma once

#include "Class1.g.h"
#include "Class2.g.h"

namespace winrt::Test::implementation
{
    struct Class1 : Class1T<Class1>
    {
        Class1() = default;

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };

    struct Class2 : Class2T<Class2>
    {
      Class2() = default;
      void f() {}
    };
}

namespace winrt::Test::factory_implementation
{
    struct Class1 : Class1T<Class1, implementation::Class1>
    {
    };

}
