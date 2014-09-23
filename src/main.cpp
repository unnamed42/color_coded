#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <iostream>

#include <clang-c/Index.h>

#include "clang/string.hpp"
#include "clang/token_pack.hpp"
#include "clang/translation_unit.hpp"
#include "clang/index.hpp"
#include "clang/resource.hpp"
#include "clang/token.hpp"

#include "ruby/vim.hpp"

#include "detail/util.hpp"
#include "detail/filesystem.hpp"

namespace color_coded
{
  void show_all_tokens(CXTranslationUnit const &tu, clang::token_pack &tokens)
  {
    ruby::vim::clearmatches();

    std::vector<CXCursor> cursors(tokens.size());
    clang_annotateTokens(tu, tokens.begin(), tokens.size(), cursors.data());

    auto cursor(cursors.cbegin());
    for(auto token(tokens.cbegin()); token != tokens.cend(); ++token, ++cursor)
    {
      CXTokenKind const kind{ clang_getTokenKind(*token) };
      clang::string const spell{ clang_getTokenSpelling(tu, *token) };
      CXSourceLocation const loc(clang_getTokenLocation(tu, *token));

      CXFile file{};
      unsigned line{}, column{}, offset{};
      clang_getFileLocation(loc, &file, &line, &column, &offset);
      clang::string const filename{ clang_getFileName(file) };

      std::string const token_text{ spell.c_str() };
      std::string const typ{ clang::token::to_string(kind, cursor->kind) };

      ruby::vim::matchaddpos(typ, line, column, token_text.size());
    }
  }

  CXSourceRange get_filerange(CXTranslationUnit const &tu, std::string const &filename)
  {
    CXFile const file{ clang_getFile(tu, filename.c_str()) };
    std::size_t const size{ detail::filesystem::file_size(filename.c_str()) };

    CXSourceLocation const top(clang_getLocationForOffset(tu, file, 0));
    CXSourceLocation const bottom(clang_getLocationForOffset(tu, file, size));
    if(clang_equalLocations(top,  clang_getNullLocation()) ||
       clang_equalLocations(bottom, clang_getNullLocation()))
    { throw std::runtime_error{ "cannot retrieve location" }; }

    CXSourceRange const range(clang_getRange(top, bottom));
    if(clang_Range_isNull(range))
    { throw std::runtime_error{ "cannot retrieve range" }; }

    return range;
  }

  void work(std::string const &name)
  {
    auto const args(detail::make_array("-std=c++1y", "-stdlib=libc++", "-I/usr/include", "-I/usr/lib/clang/3.5.0/include", "-I.", "-Iinclude", "-Ilib/juble/include", "-Ilib/juble/lib/ruby/include", "-Ilib/juble/lib/ruby/.ext/include/x86_64-linux"));

    std::string const filename{ name };
    clang::index const index{ clang_createIndex(true, true) };
    clang::translation_unit const tu
    { clang_parseTranslationUnit(index.get(), filename.c_str(),
        args.data(), args.size(), nullptr, 0, CXTranslationUnit_None) };

    std::size_t const errors{ clang_getNumDiagnostics(tu.get()) };
    if(errors || !tu.get())
    {
      for(std::size_t i{}; i != errors; ++i)
      {
        CXDiagnostic const diag{ clang_getDiagnostic(tu.get(), i) };
        clang::string const string{ clang_formatDiagnostic(diag, clang_defaultDiagnosticDisplayOptions()) };
        std::cout << string.c_str() << std::endl;
      }
      throw std::runtime_error{ "unable to compile translation unit" };
    }

    CXSourceRange const range(get_filerange(tu.get(), filename));
    clang::token_pack tp{ tu.get(), range };
    show_all_tokens(tu.get(), tp);
  }
}

extern "C" void Init_color_coded()
{
  script::registrar::add(script::func(&color_coded::work, "cc_work"));
}
