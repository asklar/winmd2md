#include "pch.h"
#include "Class.h"
#include "Class1.g.cpp"

namespace winrt::Test::implementation
{
    int32_t Class1::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void Class1::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}
