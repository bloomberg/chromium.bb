//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// XFAIL: with_system_cxx_lib=macosx10.13
// XFAIL: with_system_cxx_lib=macosx10.12
// XFAIL: with_system_cxx_lib=macosx10.11
// XFAIL: with_system_cxx_lib=macosx10.10
// XFAIL: with_system_cxx_lib=macosx10.9
// XFAIL: with_system_cxx_lib=macosx10.8
// XFAIL: with_system_cxx_lib=macosx10.7

// <istream>

// basic_istream<charT,traits>& getline(char_type* s, streamsize n);

#include <istream>
#include <cassert>

#include "test_macros.h"

template <class CharT>
struct testbuf
    : public std::basic_streambuf<CharT>
{
    typedef std::basic_string<CharT> string_type;
    typedef std::basic_streambuf<CharT> base;
private:
    string_type str_;
public:

    testbuf() {}
    testbuf(const string_type& str)
        : str_(str)
    {
        base::setg(const_cast<CharT*>(str_.data()),
                   const_cast<CharT*>(str_.data()),
                   const_cast<CharT*>(str_.data()) + str_.size());
    }

    CharT* eback() const {return base::eback();}
    CharT* gptr() const {return base::gptr();}
    CharT* egptr() const {return base::egptr();}
};

int main()
{
    {
        testbuf<char> sb("  \n    \n ");
        std::istream is(&sb);
        char s[5];
        is.getline(s, 5);
        assert(!is.eof());
        assert(!is.fail());
        assert(std::string(s) == "  ");
        assert(is.gcount() == 3);
        is.getline(s, 5);
        assert(!is.eof());
        assert(!is.fail());
        assert(std::string(s) == "    ");
        assert(is.gcount() == 5);
        is.getline(s, 5);
        assert( is.eof());
        assert(!is.fail());
        assert(std::string(s) == " ");
        assert(is.gcount() == 1);
        // Check that even in error case the buffer is properly 0-terminated.
        is.getline(s, 5);
        assert( is.eof());
        assert( is.fail());
        assert(std::string(s) == "");
        assert(is.gcount() == 0);
    }
#ifndef TEST_HAS_NO_EXCEPTIONS
    {
        testbuf<char> sb(" ");
        std::istream is(&sb);
        char s[5] = "test";
        is.exceptions(std::istream::eofbit | std::istream::badbit);
        try
        {
            is.getline(s, 5);
            assert(false);
        }
        catch (std::ios_base::failure&)
        {
        }
        assert( is.eof());
        assert( is.fail());
        assert(std::string(s) == " ");
        assert(is.gcount() == 1);
    }
#endif
    {
        testbuf<wchar_t> sb(L"  \n    \n ");
        std::wistream is(&sb);
        wchar_t s[5];
        is.getline(s, 5);
        assert(!is.eof());
        assert(!is.fail());
        assert(std::wstring(s) == L"  ");
        assert(is.gcount() == 3);
        is.getline(s, 5);
        assert(!is.eof());
        assert(!is.fail());
        assert(std::wstring(s) == L"    ");
        assert(is.gcount() == 5);
        is.getline(s, 5);
        assert( is.eof());
        assert(!is.fail());
        assert(std::wstring(s) == L" ");
        assert(is.gcount() == 1);
        // Check that even in error case the buffer is properly 0-terminated.
        is.getline(s, 5);
        assert( is.eof());
        assert( is.fail());
        assert(std::wstring(s) == L"");
        assert(is.gcount() == 0);
    }
#ifndef TEST_HAS_NO_EXCEPTIONS
    {
        testbuf<wchar_t> sb(L" ");
        std::wistream is(&sb);
        wchar_t s[5] = L"test";
        is.exceptions(std::wistream::eofbit | std::wistream::badbit);
        try
        {
            is.getline(s, 5);
            assert(false);
        }
        catch (std::ios_base::failure&)
        {
        }
        assert( is.eof());
        assert( is.fail());
        assert(std::wstring(s) == L" ");
        assert(is.gcount() == 1);
    }
#endif
}
