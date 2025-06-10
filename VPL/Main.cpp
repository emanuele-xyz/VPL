#include "PCH.h"

class Error : public std::runtime_error
{
public:
    Error(const char* file, int line, const std::string& msg)
        : std::runtime_error{ std::format("{}({}): {}\n{}", file, line, msg, std::stacktrace::current(1)) }
    {}
};

#if defined(_DEBUG)
#define Check(p) do { if (!(p)) __debugbreak(); } while (false)
#else
#define Check(p) do { if (!(p)) throw Error(__FILE__, __LINE__, "check failed: " #p); } while (false)
#endif

static void Entry()
{
    Check(false);
}

int main()
{
    try
    {
        Entry();
    }
    catch (const Error& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
